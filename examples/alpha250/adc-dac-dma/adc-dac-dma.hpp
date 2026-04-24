/// DMA driver
///
/// (c) Koheron

#ifndef __DRIVERS_ADC_DAC_DMA_HPP__
#define __DRIVERS_ADC_DAC_DMA_HPP__

#include "server/runtime/syslog.hpp"
#include "server/hardware/memory_manager.hpp"

#include <array>
#include <complex>
#include <cstdint>
#include <vector>
#include <thread>
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
    constexpr uint32_t control = 0x18;       // Control
    constexpr uint32_t status = 0x1C;        // Status
}

// System Level Control Registers
// https://www.xilinx.com/support/documentation/user_guides/ug585-Zynq-7000-TRM.pdf
namespace Sclr_regs {
    constexpr uint32_t sclr_unlock = 0x8;       // SLCR Write Protection Unlock
    constexpr uint32_t fpga0_clk_ctrl = 0x170;  // PL Clock 0 Output control
    constexpr uint32_t fpga1_clk_ctrl = 0x180;  // PL Clock 1 Output control
    constexpr uint32_t ocm_rst_ctrl = 0x238;    // OCM Software Reset Control
    constexpr uint32_t fpga_rst_ctrl = 0x240;   // FPGA Software Reset Control
    constexpr uint32_t ocm_cfg = 0x910;         // FPGA Software Reset Control
}

constexpr uint32_t n_pts = 64 * 1024; // Number of words in one descriptor
constexpr uint32_t n_desc = 256; // Number of descriptors

class AdcDacDma
{
  public:
    AdcDacDma()
    : ctl     (hw::get_memory<mem::control>())
    , dma     (hw::get_memory<mem::dma>())
    , ram_mm2s(hw::get_memory<mem::ram_mm2s>())
    , ram_s2mm(hw::get_memory<mem::ram_s2mm>())
    , axi_hp0 (hw::get_memory<mem::axi_hp0>())
    , axi_hp2 (hw::get_memory<mem::axi_hp2>())
    , ocm_mm2s(hw::get_memory<mem::ocm_mm2s>())
    , ocm_s2mm(hw::get_memory<mem::ocm_s2mm>())
    , sclr    (hw::get_memory<mem::sclr>())
    {
        // Unlock SCLR
        sclr.write<Sclr_regs::sclr_unlock>(0xDF0D);
        sclr.clear_bit<Sclr_regs::fpga_rst_ctrl, 1>();

        // Make sure that the width of the AXI HP port is 64 bit.
        axi_hp0.clear_bit<0x0, 0>();
        axi_hp0.clear_bit<0x14, 0>();
        axi_hp2.clear_bit<0x0, 0>();
        axi_hp2.clear_bit<0x14, 0>();

        // Map the last 64 kB of OCM RAM to the high address space
        sclr.write<Sclr_regs::ocm_cfg>(0b1000);

        // Optional: clear capture buffer
        for (uint32_t i = 0; i < n_pts * n_desc; i++) {
            ram_s2mm.write_reg(4*i, 0);
        }
    }

    void reset() {
        // Reset both channels
        dma.set_bit<Dma_regs::mm2s_dmacr, 2>();
        dma.set_bit<Dma_regs::s2mm_dmacr, 2>();

        // Wait for reset bits to clear (self-clearing)
        uint32_t cnt = 0;
        while (dma.read_bit<Dma_regs::mm2s_dmacr, 2>() ||
               dma.read_bit<Dma_regs::s2mm_dmacr, 2>()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (++cnt > 50) {
                log<ERROR>("DMA reset timeout\n");
                break;
            }
        }
        ctl.set_bit<reg::reset, 0>();
        ctl.clear_bit<reg::reset, 0>();
    }

    void select_adc_channel(uint32_t channel) {
        ctl.write<reg::channel_select>(channel % 2);
    }

    void set_dac_data(const std::vector<uint32_t>& dac_data) {
        auto& ram_mm2s = hw::get_memory<mem::ram_mm2s>();
        for (uint32_t i = 0; i < dac_data.size(); i++) {
            ram_mm2s.write_reg(4*i, dac_data[i]);
        }
    }

    // N in [1..n_desc]
    void start_dma(uint32_t N) {
        if (N < 1) N = 1;
        if (N > n_desc) N = n_desc;

        reset();
        ctl.write<reg::pkt_count>(N);

        set_descriptors(N);

        // Start at descriptor 0
        dma.write<Dma_regs::mm2s_curdesc>(mem::ocm_mm2s_addr);
        dma.write<Dma_regs::s2mm_curdesc>(mem::ocm_s2mm_addr);

        // Run channels
        dma.set_bit<Dma_regs::mm2s_dmacr, 0>();
        dma.set_bit<Dma_regs::s2mm_dmacr, 0>();

        // Stop after N descriptors
        dma.write<Dma_regs::mm2s_taildesc>(mem::ocm_mm2s_addr + (N-1) * 0x40);
        dma.write<Dma_regs::s2mm_taildesc>(mem::ocm_s2mm_addr + (N-1) * 0x40);

        mmio_flush();

        // Trigger
        ctl.set_bit<reg::trig, 0>();
        ctl.clear_bit<reg::trig, 0>();

    }

    void stop_dma() {
        dma.clear_bit<Dma_regs::mm2s_dmacr, 0>();
        dma.clear_bit<Dma_regs::s2mm_dmacr, 0>();
    }

    void bode_reset(uint32_t n_fft, double fs_hz);
    void bode_set_baseline(const std::vector<double>& h_real,
                           const std::vector<double>& h_imag,
                           const std::vector<uint8_t>& mask);
    void bode_clear_baseline();
    void bode_acquire_step(uint32_t N,
                           double thr_rel,
                           bool remove_delay,
                           double band_lo,
                           double band_hi,
                           bool apply_baseline);

    const auto& bode_get_h_real() const {
        return bode_state.h_real;
    }

    const auto& bode_get_h_imag() const {
        return bode_state.h_imag;
    }

    const auto& bode_get_mask() const {
        return bode_state.mask;
    }

    double bode_get_tau() const {
        return bode_state.last_tau;
    }

    uint32_t bode_get_count() const {
        return bode_state.count;
    }

    auto& get_adc_data() {
        data = ram_s2mm.read_array<uint32_t, n_desc * n_pts>();
        return data;
    }

    auto get_adc_data_n(uint32_t N) {
        if (N < 1) N = 1;
        if (N > n_desc) N = n_desc;
        return ram_s2mm.read_span<uint32_t>(N * n_pts);
    }

  private:
    hw::Memory<mem::control>&  ctl;
    hw::Memory<mem::dma>&      dma;
    hw::Memory<mem::ram_mm2s>& ram_mm2s;
    hw::Memory<mem::ram_s2mm>& ram_s2mm;
    hw::Memory<mem::axi_hp0>&  axi_hp0;
    hw::Memory<mem::axi_hp2>&  axi_hp2;
    hw::Memory<mem::ocm_mm2s>& ocm_mm2s;
    hw::Memory<mem::ocm_s2mm>& ocm_s2mm;
    hw::Memory<mem::sclr>&     sclr;

    std::array<uint32_t, n_desc * n_pts> data;

    struct BodeState {
        uint32_t n_fft = 0;
        double fs = 0.0;
        std::vector<double> window{};
        std::vector<double> freqs{};
        std::vector<double> sxx{};
        std::vector<std::complex<double>> syx{};
        std::vector<double> h_real{};
        std::vector<double> h_imag{};
        std::vector<uint8_t> mask{};
        std::vector<std::complex<double>> baseline{};
        std::vector<uint8_t> baseline_mask{};
        bool baseline_valid = false;
        double last_tau = 0.0;
        uint32_t count = 0;
    };

    BodeState bode_state;

    void mmio_flush() {
        (void)dma.read<Dma_regs::mm2s_dmasr>();
        (void)dma.read<Dma_regs::s2mm_dmasr>();
        asm volatile("dsb sy" ::: "memory");
    }

    void set_descriptors(uint32_t N) {
        // Each descriptor buffer is 4*n_pts bytes (256 KiB).
        // For safe one-shot, we build a linear chain of N descriptors.
        for (uint32_t i = 0; i < N; i++) {
            uint32_t next = (i + 1 < N) ? (i + 1) : i; // last points to itself

            // MM2S descriptors
            ocm_mm2s.write_reg(0x40 * i + Sg_regs::nxtdesc, mem::ocm_mm2s_addr + 0x40 * next);
            ocm_mm2s.write_reg(0x40 * i + Sg_regs::buffer_address, mem::ram_mm2s_addr + i * 4 * n_pts);
            ocm_mm2s.write_reg(0x40 * i + Sg_regs::control, 4 * n_pts);
            ocm_mm2s.write_reg(0x40 * i + Sg_regs::status, 0);

            // S2MM descriptors
            ocm_s2mm.write_reg(0x40 * i + Sg_regs::nxtdesc, mem::ocm_s2mm_addr + 0x40 * next);
            ocm_s2mm.write_reg(0x40 * i + Sg_regs::buffer_address, mem::ram_s2mm_addr + i * 4 * n_pts);
            ocm_s2mm.write_reg(0x40 * i + Sg_regs::control, 4 * n_pts);
            ocm_s2mm.write_reg(0x40 * i + Sg_regs::status, 0);
        }
    }

    void log_dma() {
        log("MM2S LOG \n");
        log("DMAIntErr = %d \n", dma.read_bit<Dma_regs::mm2s_dmasr, 4>());
        log("DMASlvErr = %d \n", dma.read_bit<Dma_regs::mm2s_dmasr, 5>());
        log("DMADecErr = %d \n", dma.read_bit<Dma_regs::mm2s_dmasr, 6>());
        log("SGIntErr = %d \n", dma.read_bit<Dma_regs::mm2s_dmasr, 8>());
        log("SGSlvErr = %d \n", dma.read_bit<Dma_regs::mm2s_dmasr, 9>());
        log("SGDecErr = %d \n", dma.read_bit<Dma_regs::mm2s_dmasr, 10>());
        log("CURDESC = %u \n", (dma.read<Dma_regs::mm2s_curdesc>() - mem::ocm_mm2s_addr)/0x40);
        log("\n");

        log("S2MM LOG \n");
        log("DMAIntErr = %d \n", dma.read_bit<Dma_regs::s2mm_dmasr, 4>());
        log("DMASlvErr = %d \n", dma.read_bit<Dma_regs::s2mm_dmasr, 5>());
        log("DMADecErr = %d \n", dma.read_bit<Dma_regs::s2mm_dmasr, 6>());
        log("SGIntErr = %d \n", dma.read_bit<Dma_regs::s2mm_dmasr, 8>());
        log("SGSlvErr = %d \n", dma.read_bit<Dma_regs::s2mm_dmasr, 9>());
        log("SGDecErr = %d \n", dma.read_bit<Dma_regs::s2mm_dmasr, 10>());
        log("CURDESC = %u \n", (dma.read<Dma_regs::s2mm_curdesc>() - mem::ocm_s2mm_addr)/0x40);
        log("\n");

        log("S2MM_STATUS DMAIntErr = %d \n", ocm_s2mm.read_bit<Sg_regs::status, 28>());
        log("S2MM_STATUS DMASlvErr = %d \n", ocm_s2mm.read_bit<Sg_regs::status, 29>());
        log("S2MM_STATUS DMADecErr = %d \n", ocm_s2mm.read_bit<Sg_regs::status, 30>());
        log("S2MM_STATUS Cmplt = %d \n", ocm_s2mm.read_bit<Sg_regs::status, 31>());
        log("\n");
    }

    void log_hp0() {
        log("AXI_HP0 LOG \n");
        log("AFI_WRCHAN_CTRL = %x \n", axi_hp0.read<0x14>());
        log("AFI_WRCHAN_ISSUINGCAP = %x \n", axi_hp0.read<0x18>());
        log("AFI_WRQOS = %x \n", axi_hp0.read<0x1C>());
        log("AFI_WRDATAFIFO_LEVEL = %x \n", axi_hp0.read<0x20>());
        log("AFI_WRDEBUG = %x \n", axi_hp0.read<0x24>());
        log("\n");
    }
};

#endif // __DRIVERS_ADC_DAC_DMA_HPP__
