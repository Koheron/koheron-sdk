/// PhaseNoiseAnalyzer driver
///
/// (c) Koheron

#ifndef __PHASE_NOISE_ANALYZER_HPP__
#define __PHASE_NOISE_ANALYZER_HPP__

#include <array>
#include <atomic>
#include <cstdint>
#include <complex>
#include <shared_mutex>
#include <tuple>
#include <vector>
#include <scicpp/core.hpp>
#include <scicpp/signal.hpp>

#include "server/runtime/driver_manager.hpp"
#include "server/hardware/memory_manager.hpp"
#include "boards/alpha250-4/drivers/clock-generator.hpp"

#include "./dds.hpp"
#include "./moving_averager.hpp"
#include "./phase-dma.hpp"

namespace rt { class ConfigManager; }
class DmaS2MM;
class Ltc2157;
class Dds;

class PhaseNoiseAnalyzer
{
    using Phase = scicpp::units::radian<float>;
    using Time = scicpp::units::time<float>;
    using Frequency = scicpp::units::frequency<float>;
    using PhaseNoiseDensity = scicpp::units::quantity_divide<
                scicpp::units::quantity_multiply<Phase, Phase>,
                Frequency>;
    using ComplexPhaseNoiseDensity = std::complex<PhaseNoiseDensity>;

    // FFT buffer sizes
    static constexpr uint32_t fft_size = 32768;
    static constexpr uint32_t data_size = 2 * fft_size;
    static constexpr auto calib_factor = 4.196f * scicpp::pi<Phase> / 8192.0f;

    using PhaseDataArray = std::array<Phase, data_size>;
    using PhaseNoiseDensityVector = std::vector<PhaseNoiseDensity>;

  public:
    PhaseNoiseAnalyzer();
    ~PhaseNoiseAnalyzer();

    void save_config();
    void set_local_oscillator(uint32_t channel, double freq_hz);
    void set_cic_rate(uint32_t rate);
    void set_channel(uint32_t chan);
    void set_fft_navg(uint32_t n_avg);

    auto get_parameters() {
        return std::tuple{
            fft_size / 2,
            fs,
            channel,
            cic_rate,
            fft_navg,
            dds.get_dds_freq(0),
            dds.get_dds_freq(1),
            rt::get_driver<ClockGenerator>().get_reference_clock()
        };
    }

    double get_carrier_power(uint32_t navg); // Carrier power in dBm

    auto get_jitter() {
        return std::tuple{
            phase_jitter,
            time_jitter,
            f_lo_used,
            f_hi_used
        };
    }

    auto get_measurements(uint32_t navg) {
        return std::tuple{
            phase_jitter,
            time_jitter,
            f_lo_used,
            f_hi_used,
            get_carrier_power(navg)
        };
    }

    PhaseDataArray get_phase_x();
    PhaseDataArray get_phase_y();
    PhaseNoiseDensityVector get_phase_noise() const;

  private:
    rt::ConfigManager& cfg;
    Ltc2157& ltc2157;
    Dds& dds;
    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::status>& sts;

    PhaseDma dma;

    enum InputChannel: uint32_t {
        X,   // Phase difference between IN0 and IN1
        Y,   // Phase difference between IN2 and IN3
        XY,  // Cross-spectrum between X and Y
    };

    uint32_t channel;
    uint32_t fft_navg;
    uint32_t cic_rate;
    Frequency fs_adc, fs;
    Time dma_transfer_duration;

    mutable std::shared_mutex data_mtx; // protects phase & phase_noise

    PhaseDataArray phase_x;
    PhaseDataArray phase_y;

    // Spectrum analyzer
    std::thread sa_thread;
    std::atomic<bool> spectrum_analyzer_started{false};
    scicpp::signal::Spectrum<float> spectrum;
    PhaseNoiseDensityVector phase_noise;
    MovingAverager<PhaseNoiseDensity> averager;
    MovingAverager<ComplexPhaseNoiseDensity> averager_xy;

    // Jitter (integrated noise)
    Phase phase_jitter{0.0f};
    Time time_jitter{0.0f};
    Frequency f_lo_used{0.0f}; // Integration interval start
    Frequency f_hi_used{0.0f}; // Integration interval end

    // Carrier power
    scicpp::units::dimensionless<double> conv_factor_dBm;
    std::array<scicpp::units::electric_potential<double>, 2> vrange;

    // ----------------- Private functions

    void load_config();
    void reset_phase_unwrapper();
    void update_interferometer_transfer_function();
    void set_power_conversion_factor();
    auto compute_phase_noise(PhaseDataArray& new_phase);
    auto compute_crossed_phase_noise(PhaseDataArray& new_phase_x, PhaseDataArray& new_phase_y);
    auto compute_jitter(const PhaseNoiseDensityVector& new_pn);
    void get_phase_xy();
    void start_spectrum_analyzer();
    void spectrum_analyzer_thread();
};

#endif // __PHASE_NOISE_ANALYZER_HPP__
