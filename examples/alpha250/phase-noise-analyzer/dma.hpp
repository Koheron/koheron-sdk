/// DMA driver
///
/// (c) Koheron

#ifndef __DRIVERS_DMA_HPP__
#define __DRIVERS_DMA_HPP__

#include <context.hpp>

#include <Eigen/FFT>
#include <complex>
#include <algorithm>
#include <thread>
#include <chrono>

#include <boards/alpha250/drivers/clock-generator.hpp>
#include <server/fft-windows.hpp>

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
    Dma(Context& ctx_)
    : ctx(ctx_)
    , dma(ctx.mm.get<mem::dma>())
    , ram(ctx.mm.get<mem::ram>())
    , axi_hp0(ctx.mm.get<mem::axi_hp0>())
    , clk_gen(ctx.get<ClockGenerator>())
    , phase_noise(fft_size / 2)
    , window(FFTwindow::hann, fft_size)
    {
        std::get<0>(data_vec) = std::vector<float>(fft_size);
        std::get<1>(data_vec) = std::vector<float>(fft_size);

        // Set AXI_HP0 to 32 bits
        axi_hp0.set_bit<0x0, 0>();
        axi_hp0.set_bit<0x14, 0>();

        clk_gen.set_sampling_frequency(0); // 200 MHz
        clk_gen.set_reference_clock(2);

        std::get<0>(fft).SetFlag(Eigen::FFT<float>::HalfSpectrum);
        std::get<1>(fft).SetFlag(Eigen::FFT<float>::HalfSpectrum);
    }

    auto& get_data() {
        read_dma();
        return data;
    }

    const auto& get_phase_noise();

  private:
    static constexpr uint32_t n_pts = 1024 * 1024;
    static constexpr uint32_t data_size = 1000000;
    static constexpr uint32_t fft_size = data_size / 2;
    static constexpr uint32_t read_offset = (n_pts - data_size) / 2;
    static constexpr uint32_t cic_rate = 20;

    Context& ctx;

    Memory<mem::dma>& dma;
    Memory<mem::ram>& ram;
    Memory<mem::axi_hp0>& axi_hp0;

    ClockGenerator& clk_gen;

    std::array<int32_t, data_size> data;

    std::array<Eigen::FFT<float>, 2> fft;
    std::array<std::vector<float>, 2> data_vec;
    std::array<std::vector<std::complex<float>>, 2> fft_data;
    std::vector<float> phase_noise;

    FFTwindow window;

    template<size_t idx>
    void compute_phase_noise();

    void dma_transfer() {
        reset_dma();
        start_dma();
        set_destination_address(mem::ram_addr);
        set_length(4 * n_pts);

        float fs = clk_gen.get_adc_sampling_freq();
        auto dma_duration = std::chrono::milliseconds(uint32_t(1000 * n_pts * cic_rate / 2 / fs));

        while (! dma.read_bit<Dma_regs::s2mm_dmasr, 1>()) {
            std::this_thread::sleep_for(0.95 * dma_duration);
        }

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
};

inline const auto& Dma::get_phase_noise() {
    float fs = clk_gen.get_adc_sampling_freq();

    dma_transfer();

    // Two FFTs of half a DMA buffer are computed in parallel
    auto t0 = std::thread{&Dma::compute_phase_noise<0>, this};
    auto t1 = std::thread{&Dma::compute_phase_noise<1>, this};

    t0.join();
    t1.join();

    for (uint32_t i=0; i<fft_size / 2; i++) {
        phase_noise[i] = (std::norm(std::get<0>(fft_data)[i]) + std::norm(std::get<1>(fft_data)[i])) / 2;
        phase_noise[i] /= (fs / (cic_rate * 2.0f) * window.W2()); // rad^2/Hz
    }

    return phase_noise;
}

template<size_t idx>
inline void Dma::compute_phase_noise() {
    constexpr size_t begin = idx * fft_size;
    constexpr size_t end = (idx + 1) * fft_size -1;

    float data_mean = std::accumulate(data.data() + begin, data.data() + end, 0.0) / fft_size;

    for (uint32_t i=0; i<fft_size; i++) {
        std::get<idx>(data_vec)[i] = (data[i + begin] - data_mean) * window.value(i) * float(M_PI) / 8192.0f;
    }

    std::get<idx>(fft).fwd(std::get<idx>(fft_data), std::get<idx>(data_vec));
}

#endif // __DRIVERS_DMA_HPP__
