#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"

#include <chrono>

// AXI DMA Registers
// https://www.xilinx.com/support/documentation/ip_documentation/axi_dma/v7_1/pg021_axi_dma.pdf
namespace Dma_regs {
    constexpr uint32_t mm2s_dmacr   = 0x0;   // MM2S DMA Control register
    constexpr uint32_t mm2s_dmasr   = 0x4;   // MM2S DMA Status register
    constexpr uint32_t mm2s_curdesc = 0x8;   // MM2S DMA Current Descriptor Pointer register
    constexpr uint32_t mm2s_taildesc= 0x10;  // MM2S DMA Tail Descriptor Pointer register
    constexpr uint32_t s2mm_dmacr   = 0x30;  // S2MM DMA Control register
    constexpr uint32_t s2mm_dmasr   = 0x34;  // S2MM DMA Status register
    constexpr uint32_t s2mm_curdesc = 0x38;  // S2MM DMA Current Descriptor Pointer register
    constexpr uint32_t s2mm_taildesc= 0x40;  // S2MM DMA Tail Descriptor Pointer register
}

// Scatter Gather Descriptor
namespace Sg_regs {
    constexpr uint32_t nxtdesc        = 0x0;   // Next Descriptor Pointer
    constexpr uint32_t buffer_address = 0x8;   // Buffer address
    constexpr uint32_t control        = 0x18;  // Control (length and flags)
    constexpr uint32_t status         = 0x1C;  // Status (S2MM complete bit[31], byte count [25:0])
}

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

// 64 MiB transfer via SG ring of 64 BDs (1 MiB each)
constexpr uint32_t BYTES = 64u * 1024u * 1024u;
constexpr uint32_t CHUNK = 1u * 1024u * 1024u;       // per-BD payload size
constexpr uint32_t N_BD  = BYTES / CHUNK;            // 64
constexpr uint32_t n_pts = BYTES / 4u;               // 32-bit words

// MM2S BD control flags (SOF/EOF so TLAST is generated at the very end)
constexpr uint32_t BD_CTRL_TXEOF = 1u << 26;
constexpr uint32_t BD_CTRL_TXSOF = 1u << 27;

int main() {
    using namespace std::chrono;

    FpgaManager fpga; ZynqFclk fclk; MemoryManager mm;
    if (fpga.load_bitstream() < 0) { koheron::print<PANIC>("Failed to load bitstream.\n"); return -1; }
    if (mm.open() < 0)            { koheron::print<PANIC>("Failed to open memory\n");      return -1; }

    // 100 MHz fabric clock
    fclk.set("fclk0", 100000000);
    systemd::notify_ready();

    auto& dma      = mm.get<mem::dma>();
    auto& ram_s2mm = mm.get<mem::ram_s2mm>();
    auto& ram_mm2s = mm.get<mem::ram_mm2s>();
    auto& axi_hp0  = mm.get<mem::axi_hp0>();
    auto& axi_hp2  = mm.get<mem::axi_hp2>();
    auto& ocm_mm2s = mm.get<mem::ocm_mm2s>();
    auto& ocm_s2mm = mm.get<mem::ocm_s2mm>();
    auto& sclr     = mm.get<mem::sclr>();

    // Unlock SCLR
    sclr.write<Sclr_regs::sclr_unlock>(0xDF0D);
    sclr.clear_bit<Sclr_regs::fpga_rst_ctrl, 1>();

    // Make sure that the width of the AXI HP port is 64 bit.
    axi_hp0.clear_bit<0x0, 0>();  axi_hp0.clear_bit<0x14, 0>();
    axi_hp2.clear_bit<0x0, 0>();  axi_hp2.clear_bit<0x14, 0>();

    // Map the last 64 kB of OCM RAM to the high address space
    sclr.write<Sclr_regs::ocm_cfg>(0b1000);

    // --- Prepare one-shot payload -------------------------------------------------
    // Fill TX buffer with a simple ramp; clear RX.
    for (uint32_t i = 0; i < n_pts; i++) { ram_mm2s.write_reg(4*i, i); ram_s2mm.write_reg(4*i, 0); }

    // --- Build N-descriptor ring (linked list) -----------------------------------
    static_assert(BYTES % CHUNK == 0, "BYTES must be a multiple of CHUNK");
    for (uint32_t i = 0; i < N_BD; ++i) {
        const uint32_t off = 0x40 * i;
        const uint32_t next_mm2s = mem::ocm_mm2s_addr + 0x40 * ((i + 1) % N_BD);
        const uint32_t next_s2mm = mem::ocm_s2mm_addr + 0x40 * ((i + 1) % N_BD);

        // MM2S BD i
        ocm_mm2s.write_reg(Sg_regs::nxtdesc        + off, next_mm2s);
        ocm_mm2s.write_reg(Sg_regs::buffer_address + off, mem::ram_mm2s_addr + i * CHUNK);
        uint32_t ctrl = CHUNK;
        if (i == 0)        ctrl |= BD_CTRL_TXSOF;  // one SOF at start
        if (i == N_BD - 1) ctrl |= BD_CTRL_TXEOF;  // one EOF/TLAST at end
        ocm_mm2s.write_reg(Sg_regs::control + off, ctrl);
        ocm_mm2s.write_reg(Sg_regs::status  + off, 0);

        // S2MM BD i
        ocm_s2mm.write_reg(Sg_regs::nxtdesc        + off, next_s2mm);
        ocm_s2mm.write_reg(Sg_regs::buffer_address + off, mem::ram_s2mm_addr + i * CHUNK);
        ocm_s2mm.write_reg(Sg_regs::control + off, CHUNK);
        ocm_s2mm.write_reg(Sg_regs::status  + off, 0);
    }

    asm volatile("dsb sy" ::: "memory"); // ensure descriptors visible

    // --- Prime engines (start at BD0) --------------------------------------------
    const uint32_t mm2s_bd0  = mem::ocm_mm2s_addr;
    const uint32_t s2mm_bd0  = mem::ocm_s2mm_addr;
    const uint32_t mm2s_tail = mem::ocm_mm2s_addr + 0x40 * (N_BD - 1);
    const uint32_t s2mm_tail = mem::ocm_s2mm_addr + 0x40 * (N_BD - 1);

    dma.write<Dma_regs::mm2s_curdesc>(mm2s_bd0);
    dma.write<Dma_regs::s2mm_curdesc>(s2mm_bd0);
    dma.set_bit<Dma_regs::mm2s_dmacr, 0>();   // RS=1
    dma.set_bit<Dma_regs::s2mm_dmacr, 0>();

    // Kick and wait until last BD completes
    using clock = steady_clock;
    const auto t0 = clock::now();
    dma.write<Dma_regs::mm2s_taildesc>(mm2s_tail);
    dma.write<Dma_regs::s2mm_taildesc>(s2mm_tail);

    const auto deadline = t0 + milliseconds(5000);
    while (true) {
        const uint32_t last = ocm_s2mm.read_reg(Sg_regs::status + 0x40 * (N_BD - 1));
        if (last & (1u << 31)) break;
        if (clock::now() > deadline) {
            koheron::print<ERROR>("Timeout: MM2S_SR=0x%08x S2MM_SR=0x%08x\n",
                                  dma.read<Dma_regs::mm2s_dmasr>(), dma.read<Dma_regs::s2mm_dmasr>());
            dma.clear_bit<Dma_regs::mm2s_dmacr, 0>();
            dma.clear_bit<Dma_regs::s2mm_dmacr, 0>();
            return -2;
        }
    }
    const auto t1 = clock::now();

    // --- Stop engines -------------------------------------------------------------
    dma.clear_bit<Dma_regs::mm2s_dmacr, 0>();
    dma.clear_bit<Dma_regs::s2mm_dmacr, 0>();

    // --- Full-buffer checksum (no large allocations) -----------------------------
    uint64_t sum = 0; uint32_t mismatches = 0;
    for (uint32_t i = 0; i < n_pts; ++i) {
        const uint32_t v = ram_s2mm.read_reg(4*i);
        sum += v;
        mismatches += (v != i);
    }
    const uint64_t exp_sum = (uint64_t)n_pts * (uint64_t)(n_pts - 1) / 2ull;

    // Perf
    const double us   = duration_cast<duration<double, std::micro>>(t1 - t0).count();
    const double rate = double(BYTES) / (1024.0 * 1024.0) / (us * 1e-6);

    koheron::print<INFO>("Moved %u bytes in %.3f ms -> %.2f MiB/s | checksum=%llu/%llu mismatches=%u (n_bd=%u, chunk=%u)\n",
                         BYTES, us / 1000.0, rate,
                         (unsigned long long)sum, (unsigned long long)exp_sum, mismatches,
                         N_BD, CHUNK);
    return 0;
}
