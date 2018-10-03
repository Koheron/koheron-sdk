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
#include <server/drivers/dma-s2mm.hpp>
#include <server/fft-windows.hpp>

class PhaseNoiseAnalyzer
{
  public:
    PhaseNoiseAnalyzer(Context& ctx_)
    : ctx(ctx_)
    , dma(ctx.get<DmaS2MM>())
    , clk_gen(ctx.get<ClockGenerator>())
    , ram(ctx.mm.get<mem::ram>())
    , phase_noise(fft_size / 2)
    , window(FFTwindow::hann, fft_size)
    {
        std::get<0>(data_vec) = std::vector<float>(fft_size);
        std::get<1>(data_vec) = std::vector<float>(fft_size);

        clk_gen.set_sampling_frequency(0); // 200 MHz
        clk_gen.set_reference_clock(2);

        std::get<0>(fft).SetFlag(Eigen::FFT<float>::HalfSpectrum);
        std::get<1>(fft).SetFlag(Eigen::FFT<float>::HalfSpectrum);

        fs = clk_gen.get_adc_sampling_freq();
        dma_transfer_duration = 1000 * n_pts * cic_rate / 2 / fs;
        dma.start_transfer(mem::ram_addr, 4 * n_pts);
    }

    auto& get_data() {
        dma.start_transfer(mem::ram_addr, 4 * n_pts);
        dma.wait_for_transfer(dma_transfer_duration);
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
    DmaS2MM& dma;
    ClockGenerator& clk_gen;
    Memory<mem::ram>& ram;

    float fs;
    float dma_transfer_duration;

    std::array<int32_t, data_size> data;

    std::array<Eigen::FFT<float>, 2> fft;
    std::array<std::vector<float>, 2> data_vec;
    std::array<std::vector<std::complex<float>>, 2> fft_data;
    std::vector<float> phase_noise;

    FFTwindow window;

    template<size_t idx>
    void compute_phase_noise();
};

inline const auto& PhaseNoiseAnalyzer::get_phase_noise() {
    dma.wait_for_transfer(dma_transfer_duration);
    data = ram.read_array<int32_t, data_size, read_offset>();

    // Two FFTs of half a DMA buffer are computed in parallel
    auto t0 = std::thread{&PhaseNoiseAnalyzer::compute_phase_noise<0>, this};
    auto t1 = std::thread{&PhaseNoiseAnalyzer::compute_phase_noise<1>, this};

    t0.join();
    t1.join();

    dma.start_transfer(mem::ram_addr, 4 * n_pts);

    for (uint32_t i=0; i<fft_size / 2; i++) {
        phase_noise[i] = (std::norm(std::get<0>(fft_data)[i]) + std::norm(std::get<1>(fft_data)[i])) / 2;
        phase_noise[i] /= (fs / (cic_rate * 2.0f) * window.W2()); // rad^2/Hz
    }

    return phase_noise;
}

template<size_t idx>
inline void PhaseNoiseAnalyzer::compute_phase_noise() {
    constexpr size_t begin = idx * fft_size;
    constexpr size_t end = (idx + 1) * fft_size -1;

    float data_mean = std::accumulate(data.data() + begin, data.data() + end, 0.0) / fft_size;

    for (uint32_t i=0; i<fft_size; i++) {
        std::get<idx>(data_vec)[i] = (data[i + begin] - data_mean) * window.value(i) * float(M_PI) / 8192.0f;
    }

    std::get<idx>(fft).fwd(std::get<idx>(fft_data), std::get<idx>(data_vec));
}

#endif // __DRIVERS_DMA_HPP__
