/// DMA driver
///
/// (c) Koheron

#ifndef __DRIVERS_DMA_HPP__
#define __DRIVERS_DMA_HPP__

#include <context.hpp>

#include <unsupported/Eigen/FFT>
#include <numeric>
#include <complex>
#include <algorithm>
#include <thread>
#include <chrono>
#include <tuple>

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
    , ctl(ctx.mm.get<mem::control>())
    , ram(ctx.mm.get<mem::ram>())
    , phase_noise(fft_size / 2)
    , window(FFTwindow::hann, fft_size)
    {
        std::get<0>(data_vec) = std::vector<float>(fft_size);
        std::get<1>(data_vec) = std::vector<float>(fft_size);

        clk_gen.set_sampling_frequency(0); // 200 MHz

        ctl.write_mask<reg::cordic, 0b11>(0b11); // Phase accumulator on

        std::get<0>(fft).SetFlag(Eigen::FFT<float>::HalfSpectrum);
        std::get<1>(fft).SetFlag(Eigen::FFT<float>::HalfSpectrum);

        fs_adc = clk_gen.get_adc_sampling_freq();
        fs = fs_adc / (2.0f * prm::cic_decimation_rate); // Sampling frequency (factor of 2 because of FIR)
        dma_transfer_duration = prm::n_pts / fs;
        ctx.log<INFO>("DMA transfer duration = %f s\n", double(dma_transfer_duration));
    }

    void start() {
        dma.start_transfer(mem::ram_addr, sizeof(int32_t) * prm::n_pts);
    }

    const auto get_parameters() {
        return std::make_tuple(
            fft_size / 2, // data_size
            fs
        );
    }

    const auto& get_data() {
        dma.start_transfer(mem::ram_addr, sizeof(int32_t) * prm::n_pts);
        dma.wait_for_transfer(dma_transfer_duration);
        data = ram.read_array<int32_t, data_size, read_offset>();
        return data;
    }

    const auto& get_phase_noise(uint32_t n_avg);

  private:
    static constexpr uint32_t data_size = 200000;
    static constexpr uint32_t fft_size = data_size / 2;
    static constexpr uint32_t read_offset = (prm::n_pts - data_size) / 2;

    Context& ctx;
    DmaS2MM& dma;
    ClockGenerator& clk_gen;
    Memory<mem::control>& ctl;
    Memory<mem::ram>& ram;

    float fs_adc, fs;
    float dma_transfer_duration;

    std::array<int32_t, data_size> data;

    std::array<Eigen::FFT<float>, 2> fft;
    std::array<std::vector<float>, 2> data_vec;
    std::array<std::vector<std::complex<float>>, 2> fft_data;
    std::vector<float> phase_noise;

    FFTwindow window;

    template<size_t idx>
    void compute_fft();

    void reset_phase_unwrapper() {
        ctl.write_mask<reg::cordic, 0b1100>(0b1100);
        ctl.write_mask<reg::cordic, 0b1100>(0b0000);
    }
};

auto PhaseNoiseAnalyzer::get_phase_noise(uint32_t n_avg) {
    std::fill(phase_noise.begin(), phase_noise.end(), 0.0f);

    for (uint32_t i=0; i<n_avg; i++) {
        dma.wait_for_transfer(dma_transfer_duration);
        data = ram.read_array<int32_t, data_size, read_offset>();

        // Two FFTs of half a DMA buffer are computed in parallel
        auto t0 = std::thread{&PhaseNoiseAnalyzer::compute_fft<0>, this};
        auto t1 = std::thread{&PhaseNoiseAnalyzer::compute_fft<1>, this};

        t0.join();
        t1.join();

        reset_phase_unwrapper();
        dma.start_transfer(mem::ram_addr, sizeof(int32_t) * prm::n_pts);

        for (uint32_t j=0; j<fft_size / 2; j++) {
            // Kiss FFT amplitudes must be scaled by a factor 1/2
            // https://stackoverflow.com/questions/5628056/kissfft-scaling
            phase_noise[j] += (std::norm(0.5f *std::get<0>(fft_data)[j]) + std::norm(0.5f * std::get<1>(fft_data)[j])) / 2.0f;
        }
    }

    for (uint32_t i=0; i<fft_size / 2; i++) {
        phase_noise[i] /= (fs * window.W2() * n_avg); // rad^2/Hz
    }

    return phase_noise;
}

template<size_t idx>
inline void PhaseNoiseAnalyzer::compute_fft() {
    constexpr size_t begin = idx * fft_size;
    constexpr size_t end = (idx + 1) * fft_size -1;

    float data_mean = std::accumulate(data.data() + begin, data.data() + end, 0.0) / double(fft_size);

    for (uint32_t i=0; i<fft_size; i++) {
        std::get<idx>(data_vec)[i] = (data[i + begin] - data_mean) * window[i] * float(M_PI) / 8192.0f;
    }

    std::get<idx>(fft).fwd(std::get<idx>(fft_data), std::get<idx>(data_vec));
}

#endif // __DRIVERS_DMA_HPP__
