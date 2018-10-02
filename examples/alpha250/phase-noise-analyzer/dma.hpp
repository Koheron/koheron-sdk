/// DMA driver
///
/// (c) Koheron

#ifndef __DRIVERS_DMA_HPP__
#define __DRIVERS_DMA_HPP__

#include <context.hpp>

#include <Eigen/FFT>
#include <complex>
#include <algorithm>

#include <boards/alpha250/drivers/clock-generator.hpp>

// https://www.xilinx.com/support/documentation/ip_documentation/axi_dma/v7_1/pg021_axi_dma.pdf
namespace Dma_regs {
    constexpr uint32_t s2mm_dmacr = 0x30;  // S2MM DMA Control register
    constexpr uint32_t s2mm_dmasr = 0x34;  // S2MM DMA Status register
    constexpr uint32_t s2mm_da = 0x48;     // S2MM Destination Address
    constexpr uint32_t s2mm_length = 0x58; // S2MM Buffer Length (Bytes)
}

class Dma
{
  public:
    Dma(Context& ctx)
    : dma(ctx.mm.get<mem::dma>())
    , ram(ctx.mm.get<mem::ram>())
    , axi_hp0(ctx.mm.get<mem::axi_hp0>())
    , clk_gen(ctx.get<ClockGenerator>())
    , data_vec(data_size)
    , phase_noise(data_size)
    {
        // Set AXI_HP0 to 32 bits
        axi_hp0.set_bit<0x0, 0>();
        axi_hp0.set_bit<0x14, 0>();

        clk_gen.set_sampling_frequency(0); // 200 MHz
        clk_gen.set_reference_clock(2);
    }

    auto& get_data() {
        read_dma();
        return data;
    }

    const auto& get_phase_noise() {
        float W = float(data_size); // TODO Replace by window correction factor
        float fs = clk_gen.get_adc_sampling_freq();

        read_dma();

        float data_mean = std::accumulate(data.begin(), data.end(), 0.0) / data_size;
        
        for (uint32_t i=0; i<data_size; i++) {
            data_vec[i] = (data[i] - data_mean) * float(M_PI) * 1.0f  / 8192.0f; // TODO replace 1.0f by window
        }

        fft.fwd(fft_data, data_vec);

        for (uint32_t i=0; i<data_size; i++) {
            phase_noise[i] = std::norm(fft_data[i]) / (fs / (cic_rate * 2.0f) * W); // rad^2/Hz
        }

        return phase_noise;
    }

  private:
    static constexpr uint32_t n_pts = 1024 * 1024;
    static constexpr uint32_t data_size = 1000000;
    static constexpr uint32_t read_offset = (n_pts - data_size) / 2;
    static constexpr uint32_t cic_rate = 20;

    Memory<mem::dma>& dma;
    Memory<mem::ram>& ram;
    Memory<mem::axi_hp0>& axi_hp0;

    ClockGenerator& clk_gen;

    std::array<int32_t, data_size> data;

    Eigen::FFT<float> fft;
    std::vector<float> data_vec; // Could be used also for phase noise data return
    std::vector<std::complex<float>> fft_data;
    std::vector<float> phase_noise;

    void read_dma() {
        // TODO LOCK MUTEX
        reset_dma();
        start_dma();
        set_destination_address(mem::ram_addr);
        set_length(4 * n_pts);
        data = ram.read_array<int32_t, data_size, read_offset>();
    }

    void reset_dma() {
        dma.set_bit<Dma_regs::s2mm_dmacr, 2>();
    }

    void start_dma() {
        dma.set_bit<Dma_regs::s2mm_dmacr, 0>();
    }

    void set_destination_address(uint32_t address) {
        dma.write<Dma_regs::s2mm_da>(address);
    }

    void set_length(uint32_t length) {
        dma.write<Dma_regs::s2mm_length>(length);
    }
} ;

#endif // __DRIVERS_DMA_HPP__
