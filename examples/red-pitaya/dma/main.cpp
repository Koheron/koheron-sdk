#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/context/memory_manager.hpp"
#include "server/context/fpga_manager.hpp"
#include "server/context/zynq_fclk.hpp"

#include <array>
#include <chrono>

// AXI DMA Registers
// https://www.xilinx.com/support/documentation/ip_documentation/axi_dma/v7_1/pg021_axi_dma.pdf
namespace Dma_regs {
    constexpr uint32_t mm2s_dmacr = 0x0;     // MM2S DMA Control register
    constexpr uint32_t mm2s_dmasr = 0x4;     // MM2S DMA Status register
    constexpr uint32_t mm2s_curdesc = 0x8;   // MM2S DMA Current Descriptor Pointer register
    constexpr uint32_t mm2s_taildesc = 0x10; // MM2S DMA Tail Descriptor Pointer register

    constexpr uint32_t s2mm_dmacr = 0x30;    // S2MM DMA Control register
    constexpr uint32_t s2mm_dmasr = 0x34;    // S2MM DMA Status register
    constexpr uint32_t s2mm_curdesc = 0x38;  // S2MM DMA Current Descriptor Pointer register
    constexpr uint32_t s2mm_taildesc = 0x40; // S2MM DMA Tail Descriptor Pointer register
}

// Scatter Gather Descriptor
namespace Sg_regs {
    constexpr uint32_t nxtdesc = 0x0;        // Next Descriptor Pointer
    constexpr uint32_t buffer_address = 0x8; // Buffer address
    constexpr uint32_t control = 0x18;       // Control (length and flags)
    constexpr uint32_t status = 0x1C;        // Status (S2MM complete bit[31], byte count [25:0])
}

// System Level Control Registers
// https://www.xilinx.com/support/documentation/user_guides/ug585-Zynq-7000-TRM.pdf
namespace Sclr_regs {
    constexpr uint32_t sclr_unlock = 0x8;       // SLCR Write Protection Unlock
    constexpr uint32_t fpga0_clk_ctrl = 0x170;  // PL Clock 0 Output control
    constexpr uint32_t fpga1_clk_ctrl = 0x180;  // PL Clock 1 Output control
    constexpr uint32_t ocm_rst_ctrl = 0x238;    // OCM Software Reset Control
    constexpr uint32_t fpga_rst_ctrl = 0x240;   // FPGA Software Reset Control
    constexpr uint32_t ocm_cfg = 0x910;         // OCM remap (High OCM)
}

constexpr uint32_t n_pts = 64 * 1024; // Number of words in one descriptor

int main() {
    FpgaManager fpga;
    ZynqFclk fclk;
    MemoryManager mm;

    if (fpga.load_bitstream() < 0) {
        koheron::print<PANIC>("Failed to load bitstream.\n");
        return -1;
    }

    if (mm.open() < 0) {
        koheron::print<PANIC>("Failed to open memory\n");
        return -1;
    }

    // 100 MHz fabric clock
    fclk.set("fclk0", 100000000, true);
    systemd::notify_ready();

    auto& dma      = mm.get<mem::dma>();
    auto& ram_s2mm = mm.get<mem::ram_s2mm>();
    auto& ram_mm2s = mm.get<mem::ram_mm2s>();
    auto& axi_hp0  = mm.get<mem::axi_hp0>();
    auto& axi_hp2  = mm.get<mem::axi_hp2>();
    auto& ocm_mm2s = mm.get<mem::ocm_mm2s>();
    auto& ocm_s2mm = mm.get<mem::ocm_s2mm>();
    auto& sclr     = mm.get<mem::sclr>();

    // --- PS/SLCR setup ------------------------------------------------------------
    sclr.write<Sclr_regs::sclr_unlock>(0xDF0D);
    sclr.clear_bit<Sclr_regs::fpga_rst_ctrl, 1>(); // deassert PL reset

    // Force AXI HP0/HP2 to 64-bit
    axi_hp0.clear_bit<0x0, 0>();
    axi_hp0.clear_bit<0x14, 0>();
    axi_hp2.clear_bit<0x0, 0>();
    axi_hp2.clear_bit<0x14, 0>();

    // Map last 64 kB of OCM to high address space (0xFFFF0000..0xFFFFFFFF)
    sclr.write<Sclr_regs::ocm_cfg>(0b1000);

    // --- Prepare one-shot payload -------------------------------------------------
    // Fill TX buffer with a simple ramp so we can sanity-check RX.
    for (uint32_t i = 0; i < n_pts; i++)
        ram_mm2s.write_reg(4*i, i);

    // Clear RX buffer
    for (uint32_t i = 0; i < n_pts; i++)
        ram_s2mm.write_reg(4*i, 0);

    // --- Build single descriptor per direction (ring of size 1, self-linked) -----
    const uint32_t bytes = 4 * n_pts;

    // MM2S BD @ ocm_mm2s base
    const uint32_t mm2s_bd = mem::ocm_mm2s_addr; // idx 0
    ocm_mm2s.write_reg(Sg_regs::nxtdesc + 0x40*0, mm2s_bd);                 // self-link
    ocm_mm2s.write_reg(Sg_regs::buffer_address + 0x40*0, mem::ram_mm2s_addr);
    ocm_mm2s.write_reg(Sg_regs::control + 0x40*0, bytes);
    ocm_mm2s.write_reg(Sg_regs::status  + 0x40*0, 0);

    // S2MM BD @ ocm_s2mm base
    const uint32_t s2mm_bd = mem::ocm_s2mm_addr; // idx 0
    ocm_s2mm.write_reg(Sg_regs::nxtdesc + 0x40*0, s2mm_bd);                 // self-link
    ocm_s2mm.write_reg(Sg_regs::buffer_address + 0x40*0, mem::ram_s2mm_addr);
    ocm_s2mm.write_reg(Sg_regs::control + 0x40*0, bytes);
    ocm_s2mm.write_reg(Sg_regs::status  + 0x40*0, 0);

    // --- Prime engines ------------------------------------------------------------
    dma.write<Dma_regs::mm2s_curdesc>(mm2s_bd);
    dma.write<Dma_regs::s2mm_curdesc>(s2mm_bd);

    // Run/Stop = 1 (bit 0). We do NOT enable cyclic (bit 4) -> single transfer.
    dma.set_bit<Dma_regs::mm2s_dmacr, 0>();
    dma.set_bit<Dma_regs::s2mm_dmacr, 0>();

    // Kick with tail descriptors = same BD (ring of 1)
    dma.write<Dma_regs::mm2s_taildesc>(mm2s_bd);
    dma.write<Dma_regs::s2mm_taildesc>(s2mm_bd);

    // --- Wait for S2MM BD complete (status[31] == 1) with a timeout --------------
    using namespace std::chrono;
    const auto t0 = steady_clock::now();
    bool done = false;
    while (!done) {
        const bool cmplt = ocm_s2mm.read_bit<Sg_regs::status, 31>();
        const bool err_dma_int = dma.read_bit<Dma_regs::s2mm_dmasr, 4>();
        const bool err_dma_slv = dma.read_bit<Dma_regs::s2mm_dmasr, 5>();
        const bool err_dma_dec = dma.read_bit<Dma_regs::s2mm_dmasr, 6>();
        const bool err_sg_int  = dma.read_bit<Dma_regs::s2mm_dmasr, 8>();
        const bool err_sg_slv  = dma.read_bit<Dma_regs::s2mm_dmasr, 9>();
        const bool err_sg_dec  = dma.read_bit<Dma_regs::s2mm_dmasr, 10>();

        if (cmplt) {
            done = true;
            break;
        }
        if (err_dma_int || err_dma_slv || err_dma_dec || err_sg_int || err_sg_slv || err_sg_dec) {
            koheron::print<ERROR>("S2MM error: dmasr=0x%08x\n", dma.read<Dma_regs::s2mm_dmasr>());
            break;
        }
        if (steady_clock::now() - t0 > 2s) {
            koheron::print<ERROR>("Timeout waiting for S2MM complete\n");
            break;
        }
    }

    // --- Stop engines -------------------------------------------------------------
    dma.clear_bit<Dma_regs::mm2s_dmacr, 0>();
    dma.clear_bit<Dma_regs::s2mm_dmacr, 0>();

    // --- Read back and print a few words -----------------------------------------
    uint32_t w0 = ram_s2mm.read_reg(0);
    uint32_t w1 = ram_s2mm.read_reg(4);
    uint32_t w2 = ram_s2mm.read_reg(8);
    uint32_t w3 = ram_s2mm.read_reg(12);

    koheron::print<INFO>("RX[0..3] = %08x %08x %08x %08x\n", w0, w1, w2, w3);

    // Optional quick sanity check vs TX
    uint32_t tx0 = ram_mm2s.read_reg(0);
    if (w0 == tx0)
        koheron::print<INFO>("OK: first word matches TX (0x%08x)\n", tx0);
    else
        koheron::print<WARNING>("Mismatch: TX[0]=0x%08x RX[0]=0x%08x\n", tx0, w0);

    return 0;
}
