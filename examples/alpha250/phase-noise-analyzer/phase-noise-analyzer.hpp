/// PhaseNoiseAnalyzer driver
///
/// (c) Koheron

#ifndef __PHASE_NOISE_ANALYZER_HPP__
#define __PHASE_NOISE_ANALYZER_HPP__

#include <context.hpp>

#include <array>
#include <atomic>
#include <complex>
#include <cstdint>
#include <cmath>
#include <tuple>
#include <algorithm>
#include <ranges>
#include <thread>
#include <shared_mutex>
#include <limits>

#include <scicpp/core.hpp>
#include <scicpp/polynomials.hpp>
#include <scicpp/signal.hpp>

#include <boards/alpha250/drivers/clock-generator.hpp>
#include <boards/alpha250/drivers/ltc2157.hpp>
#include <server/drivers/dma-s2mm.hpp>

#include "dds.hpp"
#include "moving_averager.hpp"

namespace {
    namespace sci = scicpp;
    namespace sig = scicpp::signal;
    namespace win = scicpp::signal::windows;
    namespace poly = scicpp::polynomial;
}

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
    , phase_noise(1 + fft_size / 2)
    , averager(1)
    {
        using namespace sci::units::literals;
        vrange= { 1_V * ltc2157.get_input_voltage_range(0),
                  1_V * ltc2157.get_input_voltage_range(1) };

        clk_gen.set_sampling_frequency(0); // 200 MHz

        ctl.write_mask<reg::cordic, 0b11>(0b11); // Phase accumulator on
        set_channel(0);
        set_fft_navg(1);

        fs_adc = sci::units::frequency<float>(clk_gen.get_adc_sampling_freq());
        set_cic_rate(prm::cic_decimation_rate_default);

        // Configure the spectrum analyzer
        spectrum.window(win::hann<float>(fft_size));
        spectrum.nthreads(2);
        spectrum.fs(fs);
        phase_noise.reserve(1 + fft_size / 2);
        start_acquisition();
    }

    void set_local_oscillator(uint32_t channel, double freq_hz) {
        dirty_cnt = 4;
        dds.set_dds_freq(channel, freq_hz);
        averager.clear();
    }

    void set_cic_rate(uint32_t rate) {
        if (rate < prm::cic_decimation_rate_min ||
            rate > prm::cic_decimation_rate_max) {
            ctx.log<ERROR>("PhaseNoiseAnalyzer: CIC rate out of range\n");
            return;
        }

        std::scoped_lock lk(dma_mtx); // block until any DMA transfer finishes

        cic_rate = rate;
        fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
        dma_transfer_duration = prm::n_pts / fs;
        ctx.logf<INFO>("DMA transfer duration = {} s\n", dma_transfer_duration.eval());
        spectrum.fs(fs);
        averager.clear();
        dirty_cnt = 2;
        ctl.write<reg::cic_rate>(cic_rate);
    }

    void set_channel(uint32_t chan) {
        if (chan != 0 && chan != 1) {
            ctx.log<ERROR>("PhaseNoiseAnalyzer: Invalid channel\n");
            return;
        }

        channel = chan;
        averager.clear();
        set_power_conversion_factor();
        ctl.write_mask<reg::cordic, 0b10000>((channel & 1) << 4);
    }

    const auto get_parameters() {
        return std::tuple{
            fft_size / 2,
            fs.eval(),
            channel,
            cic_rate,
            fft_navg
        };
    }

    // Carrier power in dBm
    auto get_carrier_power(uint32_t navg) {
        uint32_t demod_raw;
        double res = 0.0;

        for (uint32_t i=0; i<navg; ++i) {
            if (channel == 0) {
                demod_raw = sts.read<reg::demod0, uint32_t>();
            } else {
                demod_raw = sts.read<reg::demod1, uint32_t>();
            }

            // Extract real and imaginary parts and convert fix16_0 to float to obtain complex IQ signal
            const auto z = std::complex(static_cast<int16_t>(demod_raw & 0xFFFF) / 65536.0,
                                        static_cast<int16_t>((demod_raw >> 16) & 0xFFFF) / 65536.0);
            res += std::norm(z);
        }

        return 10.0 * std::log10(power_conversion_factor * res / double(navg));
    }

    auto get_jitter() {
        return std::tuple{phase_jitter, time_jitter, f_lo_used, f_hi_used};
    }

    auto get_phase() const {
        std::shared_lock lk(data_mtx);
        return phase;
    }

    auto get_phase_noise() const {
        std::shared_lock lk(data_mtx);
        return phase_noise;
    }

    void set_fft_navg(uint32_t n_avg) {
        fft_navg = n_avg;
        averager.set_navg(fft_navg);
    }

  private:
    static constexpr uint32_t data_size = 200000;
    static constexpr uint32_t fft_size = data_size / 2;
    static constexpr uint32_t read_offset = (prm::n_pts - data_size) / 2;
    static constexpr float calib_factor = 4.196f * sci::pi<float> / 8192.0f; // radians

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
    std::atomic<int32_t> dirty_cnt = 0;
    sci::units::frequency<float> fs_adc, fs;
    sci::units::time<float> dma_transfer_duration;

    std::mutex dma_mtx; // Guard DMA transfer
    mutable std::shared_mutex data_mtx; // protects phase & phase_noise

    // Data acquisition thread
    std::thread acq_thread;
    std::atomic<bool> acquisition_started{false};
    void acquisition_thread();
    void start_acquisition();
    std::array<float, data_size> phase; // rad

    // Spectrum analyzer
    sig::Spectrum<float> spectrum;
    std::vector<float> phase_noise; // rad^2/Hz
    MovingAverager<float> averager;

    // Jitter (integrated noise)
    float phase_jitter = 0.0f; // rad rms
    float time_jitter = 0.0f; // s rms
    float f_lo_used = 0.0f; // Integration interval start
    float f_hi_used = 0.0f; // Integration interval end

    // ----------------- Private functions

    void reset_phase_unwrapper() {
        ctl.write_mask<reg::cordic, 0b1100>(0b1100);
        ctl.write_mask<reg::cordic, 0b1100>(0b0000);
    }

    void kick_dma() {
        std::scoped_lock lk(dma_mtx);
        reset_phase_unwrapper();
        dma.start_transfer(mem::ram_addr, sizeof(int32_t) * prm::n_pts);
    }

    auto read_dma() {
        std::scoped_lock lk(dma_mtx);
        dma.wait_for_transfer(dma_transfer_duration);
        // ram.read_array<> should return by value (copy). If it returns a view,
        // copy it into a std::vector to decouple from the DMA destination.
        return ram.read_array<int32_t, data_size, read_offset>();
    }

    // Carrier power
    double power_conversion_factor;
    std::array<sci::units::electric_potential<double>, 2> vrange;

    void set_power_conversion_factor() {
        using namespace sci::units::literals;
        constexpr auto load = 50_Ohm;
        constexpr double magic_factor = 22.0;

        const double Hinv = poly::polyval(dds.get_dds_freq(channel),
                                          ltc2157.tf_polynomial<double>(channel));
        const auto power_conv_factor = Hinv * magic_factor * vrange[channel] * vrange[channel] / load;
        const auto conv_factor_dBm = power_conv_factor / 1_mW;
        power_conversion_factor = conv_factor_dBm.eval();

        // Dimensional analysis checks
        static_assert(sci::units::is_power<decltype(power_conv_factor)>);
        static_assert(sci::units::is_dimensionless<decltype(conv_factor_dBm)>);
    }

    auto compute_phase_noise(std::array<float, data_size>& new_phase) {
        auto phase_psd = spectrum.welch<sig::DENSITY, false>(new_phase);

        if (fft_navg > 1) {
            averager.append(std::move(phase_psd));
            return averager.average();
        } else {
            return phase_psd;
        }
    }

    auto compute_jitter(const std::vector<float>& new_pn) {
        const float f_dss = dds.get_dds_freq(channel);

        if (sci::almost_equal(f_dss, 0.0f)) {
            // No demodulation is DSS frequency is zero
            const auto NaN = std::numeric_limits<float>::quiet_NaN();
            phase_jitter = NaN;
            time_jitter  = NaN;
            f_lo_used    = NaN;
            f_hi_used    = NaN;
        } else {
            const std::size_t n_bins = new_pn.size();
            const float df = fs.eval() / float(fft_size);
            const float f_min_avail = df;
            const float f_max_avail = (n_bins - 1) * df;

            auto log10f = [](float f) {
                return std::log10(std::max(f, std::numeric_limits<float>::min()));
            };

            const float low_dec  = std::ceil(log10f(f_min_avail));
            const float high_dec = std::floor(log10f(f_max_avail));

            if (high_dec <= low_dec) {
                // No full decade: integrate whole available band (excluding DC)
                f_lo_used = f_min_avail;
                f_hi_used = f_max_avail;
                phase_jitter = sci::sqrt(sci::trapz(new_pn.begin() + 1, new_pn.end(), df));
            } else {
                f_lo_used = std::pow(10.0f, low_dec);
                f_hi_used = std::pow(10.0f, high_dec);

                std::size_t k1 = std::max(1u, static_cast<std::size_t>(std::ceil(f_lo_used / df)));
                std::size_t k2 = std::min(n_bins - 1u, static_cast<std::size_t>(std::floor(f_hi_used / df)));

                if (k2 <= k1) {
                    k1 = std::max(1u, k1);
                    k2 = std::min(n_bins - 1u, std::max(k1 + 1, k2));
                }

                phase_jitter = sci::sqrt(sci::trapz(new_pn.begin() + k1,
                                                    new_pn.begin() + k2 + 1,
                                                    df));
            }

            time_jitter = phase_jitter / (2.0f * sci::pi<float> * f_dss);
        }
    }
};

inline void PhaseNoiseAnalyzer::start_acquisition() {
    if (! acquisition_started) {
        acq_thread = std::thread{&PhaseNoiseAnalyzer::acquisition_thread, this};
        acq_thread.detach();
    }
}

inline void PhaseNoiseAnalyzer::acquisition_thread() {
    acquisition_started = true;
    kick_dma();

    while (acquisition_started) {
        using namespace sci::operators;

        auto samples = read_dma(); // blocking wait
        auto new_phase = samples * calib_factor;

        kick_dma(); // Immediately kick the next DMA so it runs while we compute

        if (dirty_cnt.load(std::memory_order_relaxed) > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            --dirty_cnt;
            averager.clear();
            continue;
        }

        auto new_pn = compute_phase_noise(new_phase);
        compute_jitter(new_pn);

        {
            std::unique_lock lk(data_mtx);
            phase = std::move(new_phase);
            phase_noise = std::move(new_pn);
        }
    }
}

#endif // __PHASE_NOISE_ANALYZER_HPP__
