#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/context/memory_manager.hpp"
#include "server/context/fpga_manager.hpp"
#include "server/context/zynq_fclk.hpp"

#include <chrono>

constexpr uint32_t n_pts = 8 * 1024 * 1024; // 32 MiB (with 32-bit words)

// MM2S (simple mode)
static constexpr uint32_t mm2s_dmacr  = 0x00; // MM2S DMA Control register
static constexpr uint32_t mm2s_dmasr  = 0x04; // MM2S DMA Status register
static constexpr uint32_t mm2s_sa     = 0x18; // MM2S Source Address
static constexpr uint32_t mm2s_length = 0x28; // MM2S Buffer Length (Bytes)

// S2MM (simple mode)
static constexpr uint32_t s2mm_dmacr  = 0x30; // S2MM DMA Control register
static constexpr uint32_t s2mm_dmasr  = 0x34; // S2MM DMA Status register
static constexpr uint32_t s2mm_da     = 0x48; // S2MM Destination Address
static constexpr uint32_t s2mm_length = 0x58; // S2MM Buffer Length (Bytes)

// Common bits
static constexpr uint32_t DMACR_RS       = 1u << 0;
static constexpr uint32_t DMACR_RESET    = 1u << 2;
static constexpr uint32_t DMACR_IOC_IE   = 1u << 12; // optional (interrupt enable)
static constexpr uint32_t DMACR_ERR_IE   = 1u << 14; // optional (interrupt enable)
static constexpr uint32_t DMASR_IOC_IRQ  = 1u << 12; // W1C
static constexpr uint32_t DMASR_DLY_IRQ  = 1u << 13; // W1C
static constexpr uint32_t DMASR_ERR_IRQ  = 1u << 14; // W1C
static constexpr uint32_t DMASR_W1C_IRQS = DMASR_IOC_IRQ | DMASR_DLY_IRQ | DMASR_ERR_IRQ;

// System Level Control Registers
// https://www.xilinx.com/support/documentation/user_guides/ug585-Zynq-7000-TRM.pdf
namespace Sclr_regs {
    constexpr uint32_t sclr_unlock    = 0x8;    // SLCR Write Protection Unlock
    constexpr uint32_t fpga0_clk_ctrl = 0x170;  // PL Clock 0 Output control
    constexpr uint32_t fpga1_clk_ctrl = 0x180;  // PL Clock 1 Output control
    constexpr uint32_t ocm_rst_ctrl   = 0x238;  // OCM Software Reset Control
    constexpr uint32_t fpga_rst_ctrl  = 0x240;  // FPGA Software Reset Control
    constexpr uint32_t ocm_cfg        = 0x910;  // OCM remap (High OCM)
}

int main() {
    using namespace std::chrono;

    FpgaManager fpga; ZynqFclk fclk; MemoryManager mm;
    if (fpga.load_bitstream() < 0) { koheron::print<PANIC>("Failed to load bitstream.\n"); return -1; }
    if (mm.open() < 0)            { koheron::print<PANIC>("Failed to open memory\n");      return -1; }

    // 100 MHz fabric clock
    fclk.set("fclk0", 100000000, true);
    systemd::notify_ready();

    auto& dma      = mm.get<mem::dma>();
    auto& ram_s2mm = mm.get<mem::ram_s2mm>();
    auto& ram_mm2s = mm.get<mem::ram_mm2s>();
    auto& axi_hp0  = mm.get<mem::axi_hp0>();
    auto& axi_hp2  = mm.get<mem::axi_hp2>();
    auto& sclr     = mm.get<mem::sclr>();

    // Unlock SCLR
    sclr.write<Sclr_regs::sclr_unlock>(0xDF0D);
    sclr.clear_bit<Sclr_regs::fpga_rst_ctrl, 1>();

    // Make sure that the width of the AXI HP port is 64 bit.
    axi_hp0.clear_bit<0x0, 0>();  axi_hp0.clear_bit<0x14, 0>();
    axi_hp2.clear_bit<0x0, 0>();  axi_hp2.clear_bit<0x14, 0>();

    // Prepare payload: ramp in TX, clear RX
    for (uint32_t i = 0; i < n_pts; i++) { ram_mm2s.write_reg(4*i, i); ram_s2mm.write_reg(4*i, 0); }
    const uint32_t bytes = 4u * n_pts; // 32 MiB

    // --- One shot DMA (simple mode, MM2S -> S2MM loopback in PL) -----------------

    // Reset both channels (bit 2) and clear stale IRQs
    dma.set_bit<mm2s_dmacr, 2>();  dma.set_bit<s2mm_dmacr, 2>();
    // Wait for reset to clear (bit 2 deasserts)
    for (int i = 0; i < 1000000 && dma.read_bit<mm2s_dmacr, 2>(); ++i) {}
    for (int i = 0; i < 1000000 && dma.read_bit<s2mm_dmacr, 2>(); ++i) {}
    dma.write<mm2s_dmasr>(DMASR_W1C_IRQS);
    dma.write<s2mm_dmasr>(DMASR_W1C_IRQS);

    // (Optional) enable IOC/ERR interrupts; harmless in polling mode
    // dma.write<mm2s_dmacr>(dma.read<mm2s_dmacr>() | DMACR_IOC_IE | DMACR_ERR_IE);
    // dma.write<s2mm_dmacr>(dma.read<s2mm_dmacr>() | DMACR_IOC_IE | DMACR_ERR_IE);

    // Program addresses
    dma.write<mm2s_sa>(mem::ram_mm2s_addr);
    dma.write<s2mm_da>(mem::ram_s2mm_addr);

    // Run/Stop = 1 (bit 0)
    dma.set_bit<mm2s_dmacr, 0>();
    dma.set_bit<s2mm_dmacr, 0>();

    // Ensure memory writes (payload) are visible
    asm volatile("dsb sy" ::: "memory");

    // Start S2MM first (arm receiver), then MM2S (sender)
    const auto t0 = steady_clock::now();
    dma.write<s2mm_length>(bytes);
    dma.write<mm2s_length>(bytes);

    // Wait for S2MM IOC (or timeout)
    const auto deadline = t0 + milliseconds(5000);
    while (true) {
        const uint32_t sr = dma.read<s2mm_dmasr>();
        if (sr & DMASR_IOC_IRQ) break;
        if (steady_clock::now() > deadline) {
            koheron::print<ERROR>("Timeout: MM2S_SR=0x%08x S2MM_SR=0x%08x\n",
                                  dma.read<mm2s_dmasr>(), sr);
            // Stop channels
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

    // --- Verify (checksum) --------------------------------------------------------
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n_pts; ++i) sum += ram_s2mm.read_reg(4*i);
    const uint64_t exp_sum = (uint64_t)n_pts * (uint64_t)(n_pts - 1) / 2ull;

    const double us   = duration_cast<duration<double, std::micro>>(t1 - t0).count();
    const double rate = double(bytes) / (1024.0 * 1024.0) / (us * 1e-6);

    koheron::print<INFO>("Moved %u bytes in %.3f ms -> %.2f MiB/s | checksum=%llu/%llu\n",
                         bytes, us / 1000.0, rate,
                         (unsigned long long)sum, (unsigned long long)exp_sum);

    return 0;
}
