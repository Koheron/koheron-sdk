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
    using Phase = sci::units::radian<float>;
    using Time = sci::units::time<float>;
    using Frequency = sci::units::frequency<float>;
    using PhaseNoiseDensity = sci::units::quantity_divide<
                sci::units::quantity_multiply<Phase, Phase>,
                Frequency>;

    static constexpr uint32_t fft_size = 32768;
    static constexpr uint32_t data_size = 2 * fft_size;
    static constexpr uint32_t read_offset = (prm::n_pts - data_size) / 2; // Do use the first transfered points
    static constexpr auto calib_factor = 4.196f * sci::pi<Phase> / 8192.0f;

    using PhaseDataArray = std::array<Phase, data_size>;
    using PhaseNoiseDensityVector = std::vector<PhaseNoiseDensity>;


  public:
    // PhaseNoiseAnalyzer(Context& ctx_);
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
    , interferometer_tf(1 + fft_size / 2)
    {
        using namespace sci::units::literals;
        vrange= { 1_V * ltc2157.get_input_voltage_range(0),
                  1_V * ltc2157.get_input_voltage_range(1) };

        clk_gen.set_sampling_frequency(0); // 200 MHz

        ctl.write_mask<reg::cordic, 0b11>(0b11); // Phase accumulator on
        set_channel(0);
        set_fft_navg(1);

        fs_adc = Frequency(clk_gen.get_adc_sampling_freq());
        set_cic_rate(prm::cic_decimation_rate_default);

        // Configure the spectrum analyzer
        spectrum.window(win::hann<float>(fft_size));
        spectrum.nthreads(2);
        spectrum.fs(fs);
        phase_noise.reserve(1 + fft_size / 2);
        start_acquisition();
    }

    void set_local_oscillator(uint32_t channel, double freq_hz);
    void set_cic_rate(uint32_t rate);
    void set_channel(uint32_t chan);

    auto get_parameters() {
        return std::tuple{
            fft_size / 2,
            fs,
            channel,
            cic_rate,
            fft_navg,
            dds.get_dds_freq(0),
            dds.get_dds_freq(1),
            analyzer_mode
        };
    }

    // Carrier power in dBm
    double get_carrier_power(uint32_t navg);

    auto get_jitter() {
        return std::tuple{
            phase_jitter,
            time_jitter,
            f_lo_used,
            f_hi_used
        };
    }

    PhaseDataArray get_phase() const;
    PhaseNoiseDensityVector get_phase_noise() const;

    void set_fft_navg(uint32_t n_avg);
    void set_analyzer_mode(uint32_t mode);
    void set_interferometer_delay(float delay_s);
  private:


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
    Frequency fs_adc, fs;
    Time dma_transfer_duration;

    std::mutex dma_mtx; // Guard DMA transfer
    mutable std::shared_mutex data_mtx; // protects phase & phase_noise

    // Data acquisition thread
    std::thread acq_thread;
    std::atomic<bool> acquisition_started{false};
    void acquisition_thread();
    void start_acquisition();

    PhaseDataArray phase;

    // Spectrum analyzer
    sig::Spectrum<float> spectrum;
    PhaseNoiseDensityVector phase_noise;
    MovingAverager<PhaseNoiseDensity> averager;

    // Jitter (integrated noise)
    Phase phase_jitter{0.0f};
    Time time_jitter{0.0f};
    Frequency f_lo_used{0.0f}; // Integration interval start
    Frequency f_hi_used{0.0f}; // Integration interval end

    // Laser phase noise
    enum AnalyzerMode: uint32_t {
        RF,   // Return the RF signal phase noise
        LASER // Return the laser phase noise (compensate for interferometer response)
    };

    uint32_t analyzer_mode = AnalyzerMode::RF;
    Time interferometer_delay{0.0f};
    std::vector<float> interferometer_tf; // Interferometer transfer function

    // Carrier power
    sci::units::dimensionless<double> conv_factor_dBm;
    std::array<sci::units::electric_potential<double>, 2> vrange;

    // ----------------- Private functions

    void reset_phase_unwrapper();
    void kick_dma();
    auto read_dma();
    void update_interferometer_transfer_function();
    void set_power_conversion_factor();
    auto compute_phase_noise(PhaseDataArray& new_phase);
    auto compute_jitter(const PhaseNoiseDensityVector& new_pn);
};

#endif // __PHASE_NOISE_ANALYZER_HPP__
