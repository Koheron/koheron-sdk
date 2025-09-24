/// PhaseNoiseAnalyzer driver
///
/// (c) Koheron

#ifndef __PHASE_NOISE_ANALYZER_HPP__
#define __PHASE_NOISE_ANALYZER_HPP__

#include <context.hpp>

#include <array>
#include <complex>
#include <cstdint>
#include <cmath>
#include <tuple>
#include <algorithm>
#include <ranges>
#include <thread>
#include <shared_mutex>

#include <scicpp/core.hpp>
#include <scicpp/polynomials.hpp>
#include <scicpp/signal.hpp>

#include <boards/alpha250/drivers/clock-generator.hpp>
#include <boards/alpha250/drivers/ltc2157.hpp>
#include <server/drivers/dma-s2mm.hpp>
#include <dds.hpp>

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
        ctx.logf<INFO>("DMA transfer duration = {} s\n", dma_transfer_duration.eval());
        spectrum.fs(fs);
        ctl.write<reg::cic_rate>(cic_rate);
    }

    void set_channel(uint32_t chan) {
        if (chan != 0 && chan != 1) {
            ctx.log<ERROR>("PhaseNoiseAnalyzer: Invalid channel\n");
            return;
        }

        channel = chan;
        set_power_conversion_factor();
        ctl.write_mask<reg::cordic, 0b10000>((channel & 1) << 4);
    }

    const auto get_parameters() {
        return std::make_tuple(
            fft_size / 2,
            fs.eval(),
            channel,
            cic_rate,
            fft_navg
        );
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

    auto get_phase() const {
        std::shared_lock lk(mtx);
        return phase;
    }

    auto get_phase_noise() const {
        std::shared_lock lk(mtx);
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
    static constexpr float calib_factor = 4.196f * sci::pi<float> / 8192.0f;

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
    sci::units::frequency<float> fs_adc, fs;
    sci::units::time<float> dma_transfer_duration;

    // Data acquisition thread
    std::thread acq_thread;
    mutable std::shared_mutex mtx;   // protects phase & phase_noise
    std::atomic<bool> acquisition_started{false};
    void acquisition_thread();
    void start_acquisition();
    std::array<float, data_size> phase;

    // Spectrum analyzer
    sig::Spectrum<float> spectrum;
    std::vector<float> phase_noise;
    MovingAverager<float> averager;

    void reset_phase_unwrapper() {
        ctl.write_mask<reg::cordic, 0b1100>(0b1100);
        ctl.write_mask<reg::cordic, 0b1100>(0b0000);
    }

    auto get_data() {
        reset_phase_unwrapper();
        dma.start_transfer(mem::ram_addr, sizeof(int32_t) * prm::n_pts);
        dma.wait_for_transfer(dma_transfer_duration);
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
        static_assert(sci::units::is_power<decltype(power_conv_factor)>);
        const auto conv_factor_dBm = power_conv_factor / 1_mW;
        static_assert(sci::units::is_dimensionless<decltype(conv_factor_dBm)>);
        power_conversion_factor = conv_factor_dBm.eval();
    }

    auto compute_phase_noise_from(std::array<float, data_size>& new_phase) {
        if (fft_navg > 1) {
            averager.append(spectrum.welch<sig::DENSITY, false>(new_phase));
            return averager.average();
        } else {
            return spectrum.welch<sig::DENSITY, false>(new_phase);
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

    while (acquisition_started) {
        using namespace sci::operators;
        auto new_phase = get_data() * calib_factor;
        auto new_pn = compute_phase_noise_from(new_phase);

        {
            std::unique_lock lk(mtx);
            phase = std::move(new_phase);
            phase_noise = std::move(new_pn);
        }
    }
}

#endif // __PHASE_NOISE_ANALYZER_HPP__
