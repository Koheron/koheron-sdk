#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>

using namespace std::chrono;

constexpr uint32_t n_pts  = 8 * 1024 * 1024;          // 32 MiB (with 32-bit words)
static constexpr uint32_t bytes = 4u * n_pts;         // 32 MiB

// MM2S (simple mode)
static constexpr uint32_t mm2s_dmacr  = 0x00; // DMA Control
static constexpr uint32_t mm2s_dmasr  = 0x04; // DMA Status
static constexpr uint32_t mm2s_sa     = 0x18; // Source Address
static constexpr uint32_t mm2s_length = 0x28; // Buffer Length (bytes)

// S2MM (simple mode)
static constexpr uint32_t s2mm_dmacr  = 0x30; // DMA Control
static constexpr uint32_t s2mm_dmasr  = 0x34; // DMA Status
static constexpr uint32_t s2mm_da     = 0x48; // Destination Address
static constexpr uint32_t s2mm_length = 0x58; // Buffer Length (bytes)

// Common bits
static constexpr uint32_t DMACR_RESET    = 1u << 2;
static constexpr uint32_t DMACR_IOC_IE   = 1u << 12; // (unused here)
static constexpr uint32_t DMACR_ERR_IE   = 1u << 14; // (unused here)
static constexpr uint32_t DMASR_IOC_IRQ  = 1u << 12; // W1C
static constexpr uint32_t DMASR_DLY_IRQ  = 1u << 13; // W1C
static constexpr uint32_t DMASR_ERR_IRQ  = 1u << 14; // W1C
static constexpr uint32_t DMASR_W1C_IRQS = DMASR_IOC_IRQ | DMASR_DLY_IRQ | DMASR_ERR_IRQ;

// System Level Control Registers
namespace Sclr_regs {
    constexpr uint32_t sclr_unlock    = 0x8;
    constexpr uint32_t fpga_rst_ctrl  = 0x240;
}

static double mib_per_s(size_t nbytes, duration<double> dt) {
    return double(nbytes) / (1024.0 * 1024.0) / dt.count();
}

int main() {
    hw::FpgaManager fpga; hw::ZynqFclk fclk; hw::MemoryManager mm;
    if (fpga.load_bitstream() < 0) { rt::print<PANIC>("Failed to load bitstream.\n"); return -1; }
    if (mm.open() < 0)            { rt::print<PANIC>("Failed to open memory\n");      return -1; }

    // 100 MHz fabric clock
    fclk.set("fclk0", 100000000);
    rt::systemd::notify_ready();

    auto& dma      = mm.get<mem::dma>();
    auto& ram_s2mm = mm.get<mem::ram_s2mm>(); // WC in YAML
    auto& ram_mm2s = mm.get<mem::ram_mm2s>(); // WC in YAML
    auto& axi_hp0  = mm.get<mem::axi_hp0>();
    auto& axi_hp2  = mm.get<mem::axi_hp2>();
    auto& sclr     = mm.get<mem::sclr>();

    // Deassert PL reset
    sclr.write<Sclr_regs::sclr_unlock>(0xDF0D);
    sclr.clear_bit<Sclr_regs::fpga_rst_ctrl, 1>();

    // Ensure AXI HP ports are 64-bit wide
    axi_hp0.clear_bit<0x0, 0>();  axi_hp0.clear_bit<0x14, 0>();
    axi_hp2.clear_bit<0x0, 0>();  axi_hp2.clear_bit<0x14, 0>();

    // ------------------- Prepare payloads with write_array() -------------------
    using Buf = std::array<uint32_t, n_pts>;
    auto tx   = std::make_unique<Buf>();    // ramp 0..N-1
    auto zero = std::make_unique<Buf>();    // zeros
    for (uint32_t i = 0; i < n_pts; ++i)
        (*tx)[i] = i;
    zero->fill(0u);

    const auto t_prep0 = steady_clock::now();
    ram_mm2s.write_array<uint32_t, n_pts, 0, true>(*tx);              // block_idx = 0 by default
    const auto t_prep1 = steady_clock::now();

    const auto t_clr0 = steady_clock::now();
    ram_s2mm.write_array<uint32_t, n_pts, 0, true>(*zero);            // clear RX buffer
    const auto t_clr1 = steady_clock::now();

    // Make sure WC writes are visible to DMA
    asm volatile("dsb sy" ::: "memory");

    rt::print<INFO>("Prep TX (write_array): %.2f MiB/s | Clear RX: %.2f MiB/s\n",
                         mib_per_s(bytes, t_prep1 - t_prep0),
                         mib_per_s(bytes, t_clr1  - t_clr0));

    // ------------------------ One-shot DMA (simple) ----------------------------
    // Reset channels + clear stale IRQs
    dma.set_bit<mm2s_dmacr, 2>();  dma.set_bit<s2mm_dmacr, 2>();
    for (int i = 0; i < 1000000 && dma.read_bit<mm2s_dmacr, 2>(); ++i) {}
    for (int i = 0; i < 1000000 && dma.read_bit<s2mm_dmacr, 2>(); ++i) {}
    dma.write<mm2s_dmasr>(DMASR_W1C_IRQS);
    dma.write<s2mm_dmasr>(DMASR_W1C_IRQS);

    // Program addresses
    dma.write<mm2s_sa>(mem::ram_mm2s_addr);
    dma.write<s2mm_da>(mem::ram_s2mm_addr);

    // Run/Stop = 1
    dma.set_bit<mm2s_dmacr, 0>();
    dma.set_bit<s2mm_dmacr, 0>();

    // Start S2MM first (arm receiver), then MM2S
    const auto t0 = steady_clock::now();
    dma.write<s2mm_length>(bytes);
    dma.write<mm2s_length>(bytes);

    // Wait for S2MM IOC (or timeout)
    const auto deadline = t0 + milliseconds(5000);
    while (true) {
        const uint32_t sr = dma.read<s2mm_dmasr>();
        if (sr & DMASR_IOC_IRQ) break;
        if (steady_clock::now() > deadline) {
            rt::print<ERROR>("Timeout: MM2S_SR=0x%08x S2MM_SR=0x%08x\n",
                                  dma.read<mm2s_dmasr>(), sr);
            dma.clear_bit<mm2s_dmacr, 0>();
            dma.clear_bit<s2mm_dmacr, 0>();
            return -2;
        }
    }
    const auto t1 = steady_clock::now();

    // Stop channels and clear IRQs
    dma.clear_bit<mm2s_dmacr, 0>();
    dma.clear_bit<s2mm_dmacr, 0>();
    dma.write<mm2s_dmasr>(DMASR_W1C_IRQS);
    dma.write<s2mm_dmasr>(DMASR_W1C_IRQS);

    // --------------------------- Verify (checksum) -----------------------------
    // Read back the whole RX buffer in one shot
    auto& rx = ram_s2mm.template read_array<uint32_t, n_pts>(0);
    uint64_t sum = 0;
    for (auto v : rx) sum += v;
    const uint64_t exp_sum = (uint64_t)n_pts * (uint64_t)(n_pts - 1) / 2ull;

    const double us   = duration_cast<duration<double, std::micro>>(t1 - t0).count();
    const double rate = double(bytes) / (1024.0 * 1024.0) / (us * 1e-6);

    rt::print<INFO>("DMA moved %u bytes in %.3f ms -> %.2f MiB/s | checksum=%llu/%llu\n",
                         bytes, us / 1000.0, rate,
                         (unsigned long long)sum, (unsigned long long)exp_sum);

    return 0;
}
