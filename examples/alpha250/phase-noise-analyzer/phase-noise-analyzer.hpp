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
#include <boards/alpha250/drivers/ltc2157.hpp>
#include <server/drivers/dma-s2mm.hpp>
#include <server/fft-windows.hpp>
#include <dds.hpp>

class PhaseNoiseAnalyzer
{
  public:
    PhaseNoiseAnalyzer(Context& ctx_)
    : ctx(ctx_)
    , dma(ctx.get<DmaS2MM>())
    , clk_gen(ctx.get<ClockGenerator>())
    , ltc2157(ctx.get<Ltc2157>())
    , dds(ctx.get<Dds>())
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , ram(ctx.mm.get<mem::ram>())
    , phase_noise(fft_size / 2)
    , window(FFTwindow::hann, fft_size)
    {
        std::get<0>(data_vec) = std::vector<float>(fft_size);
        std::get<1>(data_vec) = std::vector<float>(fft_size);

        vrange= { ltc2157.get_input_voltage_range(0),
                  ltc2157.get_input_voltage_range(1) };

        clk_gen.set_sampling_frequency(0); // 200 MHz

        ctl.write_mask<reg::cordic, 0b11>(0b11); // Phase accumulator on
        set_channel(0);

        set_fft_navg(1);
        std::get<0>(fft).SetFlag(Eigen::FFT<float>::HalfSpectrum);
        std::get<1>(fft).SetFlag(Eigen::FFT<float>::HalfSpectrum);

        fs_adc = clk_gen.get_adc_sampling_freq();
        set_cic_rate(prm::cic_decimation_rate_default);
        dma_transfer_duration = prm::n_pts / fs;
        ctx.log<INFO>("DMA transfer duration = %f s\n", double(dma_transfer_duration));
    }

    void start() {
        dma.start_transfer(mem::ram_addr, sizeof(int32_t) * prm::n_pts);
    }

    void set_cic_rate(uint32_t rate) {
        if (rate < prm::cic_decimation_rate_min ||
            rate > prm::cic_decimation_rate_max) {
            ctx.log<ERROR>("PhaseNoiseAnalyzer: CIC rate out of range\n");
            return;
        }

        cic_rate = rate;
        fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
        dma_transfer_duration = prm::n_pts / fs;
        ctl.write<reg::cic_rate>(cic_rate);
    }

    void set_channel(uint32_t chan) {
        if (chan != 0 && chan != 1) {
            ctx.log<ERROR>("PhaseNoiseAnalyzer: Invalid channel\n");
            return;
        }

        channel = chan;
        ctl.write_mask<reg::cordic, 0b10000>((channel & 1) << 4);
    }

    const auto get_parameters() {
        return std::make_tuple(
            fft_size / 2, // data_size
            fs,
            channel,
            cic_rate,
            fft_navg
        );
    }

    // Carrier power in dBm
    auto get_carrier_power(uint32_t navg) {
        uint32_t demod_raw;
        float res = 0.0;

        for (uint32_t i=0; i<navg; ++i) {
            if (channel == 0) {
                demod_raw = sts.read<reg::demod0, uint32_t>();
            } else {
                demod_raw = sts.read<reg::demod1, uint32_t>();
            }

            // Extract real and imaginary parts and convert fix16_0 to float to obtain complex IQ signal
            const auto z = std::complex(static_cast<int16_t>(demod_raw & 0xFFFF) / 65536.0f,
                                        static_cast<int16_t>((demod_raw >> 16) & 0xFFFF) / 65536.0f);
            res += std::norm(z);
        }

        const auto cal_coeffs = ltc2157.get_calibration(channel);
        const float fdds = dds.get_dds_freq(channel);
        const float Hinv = cal_coeffs[2] * fdds * fdds * fdds * fdds * fdds
                        + cal_coeffs[3] * fdds * fdds * fdds * fdds
                        + cal_coeffs[4] * fdds * fdds * fdds
                        + cal_coeffs[5] * fdds * fdds
                        + cal_coeffs[6] * fdds
                        + cal_coeffs[7];
        constexpr float load = 50.0; // Ohm
        constexpr float magic_factor = 22.0;
        const float conv_factor = 1E3f * magic_factor * Hinv * vrange[channel] * vrange[channel] / load;
        // ctx.log<INFO>("PhaseNoiseAnalyzer: vrange = %f, Hinv = %f, conv_factor = %f\n", double(vrange[channel]), double(Hinv), double(conv_factor));

        return 10.0f * std::log10(conv_factor * res / float(navg));
    }

    auto get_data() {
        dma.start_transfer(mem::ram_addr, sizeof(int32_t) * prm::n_pts);
        dma.wait_for_transfer(dma_transfer_duration);
        data = ram.read_array<int32_t, data_size, read_offset>();
        return data;
    }

    auto get_phase() {
        get_data();

        for (uint32_t i=0; i<data_size; i++) {
            data_phase[i] = data[i] * calib_factor * float(M_PI) / 8192.0f;
        }

        return data_phase;
    }

    void set_fft_navg(uint32_t n_avg) {
        fft_navg = n_avg;
    }

    auto get_phase_noise();

  private:
    static constexpr uint32_t data_size = 200000;
    static constexpr uint32_t fft_size = data_size / 2;
    static constexpr uint32_t read_offset = (prm::n_pts - data_size) / 2;
    static constexpr float calib_factor = 4.196;

    Context& ctx;
    DmaS2MM& dma;
    ClockGenerator& clk_gen;
    Ltc2157& ltc2157;
    Dds& dds;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::ram>& ram;

    uint32_t channel;
    uint32_t fft_navg;
    uint32_t cic_rate;
    float fs_adc, fs;
    float dma_transfer_duration;

    std::array<float, 2> vrange;

    std::array<int32_t, data_size> data;
    std::array<float, data_size> data_phase;

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

inline auto PhaseNoiseAnalyzer::get_phase_noise() {
    std::fill(phase_noise.begin(), phase_noise.end(), 0.0f);

    for (uint32_t i=0; i<fft_navg; i++) {
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
            phase_noise[j] += (std::norm(std::get<0>(fft_data)[j]) + std::norm(std::get<1>(fft_data)[j])) / 2.0f;
        }
    }

    for (uint32_t i=0; i<fft_size / 2; i++) {
        phase_noise[i] /= (fs * window.W2() * fft_navg); // rad^2/Hz
    }

    return phase_noise;
}

template<size_t idx>
inline void PhaseNoiseAnalyzer::compute_fft() {
    constexpr size_t begin = idx * fft_size;
    constexpr size_t end = (idx + 1) * fft_size -1;

    const float data_mean = std::accumulate(data.data() + begin, data.data() + end, 0.0) / double(fft_size);

    for (uint32_t i=0; i<fft_size; i++) {
        std::get<idx>(data_vec)[i] = (data[i + begin] - data_mean) * window[i] * calib_factor * float(M_PI) / 8192.0f;
    }

    std::get<idx>(fft).fwd(std::get<idx>(fft_data), std::get<idx>(data_vec));
}

#endif // __DRIVERS_DMA_HPP__
