/// PhaseNoiseAnalyzer driver
///
/// (c) Koheron

#ifndef __PHASE_NOISE_ANALYZER_HPP__
#define __PHASE_NOISE_ANALYZER_HPP__

#include <array>
#include <atomic>
#include <cstdint>
#include <complex>
#include <cmath>
#include <chrono>
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
#include "./cumulative_averager.hpp"
#include "./phase-dma.hpp"
#include "./phase_scaling.hpp"
#include "./phase-processing.hpp"
#include "./tracking_lock.hpp"

namespace rt { class ConfigManager; }
class Ltc2157;
class Dds;

class PhaseNoiseAnalyzer
{
    using Phase = scicpp::units::radian<float>;
    using Time = scicpp::units::time<double>;
    using Frequency = scicpp::units::frequency<double>;
    using PhaseNoiseDensity = scicpp::units::quantity_divide<
                scicpp::units::quantity_multiply<Phase, Phase>,
                scicpp::units::frequency<float>>;
    using ComplexPhaseNoiseDensity = std::complex<PhaseNoiseDensity>;

    // FFT buffer sizes
    static constexpr uint32_t fft_size = 32768;
    static constexpr uint32_t data_size = 2 * fft_size;
    static constexpr uint32_t spectrum_samples = 30000;
    static constexpr uint32_t spectrum_bins = spectrum_samples / 2 + 1;
    // Standard precision retains the pi/8192 CORDIC scale and the fixed-point
    // FIR DC gain of 1/4. Additional retained bits divide radians/count by 2^bits.
    static constexpr auto calib_factor = 4.0f * scicpp::pi<Phase> / 8192.0f;

    static constexpr uint32_t fifo_depth = 32768;
    static constexpr std::size_t discard_acquisitions_after_reset = 2 * ((fifo_depth + PhaseDma::samples_per_chunk - 1) / PhaseDma::samples_per_chunk);

    using PhaseDataArray = std::array<Phase, data_size>;
    using PhaseNoiseDensityVector = std::vector<PhaseNoiseDensity>;

  public:
    PhaseNoiseAnalyzer();
    ~PhaseNoiseAnalyzer();

    void save_config();
    void set_local_oscillator(uint32_t channel, double freq_hz);
    void set_cic_rate(uint32_t rate);
    void set_min_frequency(float min_frequency_hz);
    void set_channel(uint32_t chan);
    void set_fft_navg(uint32_t n_avg);
    void reset_cumulative_averager();
    auto get_nominal_frequencies() {
        std::shared_lock lk(data_mtx);
        return std::tuple{base_dds_freq[0].eval(), base_dds_freq[1].eval(),
                          base_dds_freq[2].eval(), base_dds_freq[3].eval()};
    }
    auto get_average_status() const {
        std::shared_lock lk(publication_mtx);
        return std::tuple{published_count, published_target};
    }
    void set_tracking_enabled(bool enabled);
    void set_tracking_bandwidth(float bandwidth_hz);
    void set_tracking_max_correction(float max_correction_hz);
    void set_tracking_max_step(float max_step_hz);

    auto get_tracking_parameters() {
        std::shared_lock lk(data_mtx);

        return std::tuple{
            tracking_enabled,
            tracking_bandwidth,
            effective_tracking_bandwidth(),
            tracking_correction[DdsChannel::DUTX],
            tracking_correction[DdsChannel::DUTY],
            tracking_last_mean_dphi,
            tracking_last_error,
            tracking_locked
        };
    }

    auto get_parameters() {
        std::shared_lock lk(data_mtx);
        return std::tuple{
            spectrum_bins,
            fs,
            channel,
            min_frequency,
            fft_navg,
            dds.get_dds_freq(0),
            dds.get_dds_freq(1),
            dds.get_dds_freq(2),
            dds.get_dds_freq(3),
            rt::get_driver<ClockGenerator>().get_reference_clock(),
            averager_xy.count()
        };
    }

    double get_carrier_power(uint32_t navg); // Carrier power in dBm

    auto get_jitter() {
        std::shared_lock lk(data_mtx);
        return std::tuple{
            phase_jitter,
            time_jitter,
            f_lo_used,
            f_hi_used
        };
    }

    auto get_measurements(uint32_t navg) {
        std::shared_lock lk(data_mtx);
        return std::tuple{
            phase_jitter,
            time_jitter,
            f_lo_used,
            f_hi_used,
            carrier_power(navg)
        };
    }

    PhaseDataArray get_phase_x();
    PhaseDataArray get_phase_y();
    std::array<Phase, 2 * data_size> get_phase_xy_sync();
    PhaseNoiseDensityVector get_phase_noise() const;
    bool set_phase_precision(uint32_t bits);
    auto get_precision_status() {
        std::shared_lock lk(data_mtx);
        return std::tuple{phase_precision, captured_precision,
            double(calib_factor.eval()) * cic_output_scale * phase_scale_x * std::exp2(-double(phase_precision)),
            capture_state, accepted_captures, overflow_captures, dma_errors, processing_ms, capture_period_ms};
    }
    auto get_phase_snapshot() {
        using namespace scicpp::operators;
        std::shared_lock lk(data_mtx);
        return std::tuple{accepted_captures, captured_precision, capture_state == Valid, phase_x | phase_y};
    }

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

    enum DdsChannel: uint32_t {
        DUTX, // Frequency of the X channel DUT
        REFX, // Frequency of the X channel reference
        DUTY, // Frequency of the Y channel DUT
        REFY, // Frequency of the Y channel reference
    };

    uint32_t channel = X;
    uint32_t fft_navg = 1;
    uint32_t cic_rate = prm::cic_decimation_rate_default;
    Frequency min_frequency;
    Frequency fs_adc, fs;
    Time dma_transfer_duration;

    mutable std::shared_mutex data_mtx; // protects settings, snapshots and spectral state
    // PSD reads use a short publication lock instead of waiting for the FFT.
    // Writers acquire data_mtx before publication_mtx; readers need only one.
    mutable std::shared_mutex publication_mtx;
    PhaseNoiseDensityVector published_phase_noise = PhaseNoiseDensityVector(spectrum_bins);
    uint32_t published_count = 0;
    uint32_t published_target = 1; // Zero denotes cumulative XY averaging.
    void publish_spectrum();

    PhaseDataArray phase_x{};
    PhaseDataArray phase_y{};
    double phase_scale_x = 1.0, phase_scale_y = 1.0;
    double cic_output_scale = 1.0;
    uint64_t acquisition_epoch = 0;
    uint32_t phase_precision = 0, captured_precision = 0;
    enum CaptureState : uint32_t { Settling, Valid, Overrange, DmaError };
    uint32_t capture_state = Settling;
    uint32_t hardware_epoch = 0;
    uint64_t accepted_captures = 0, overflow_captures = 0, dma_errors = 0;
    double processing_ms = 0.0, capture_period_ms = 0.0;
    std::chrono::steady_clock::time_point last_capture_time{};
    std::chrono::steady_clock::time_point last_overrange_reset{};
    void restart_filters();
    using SpectrumPhaseArray = std::array<Phase, 32000>;

    // Spectrum analyzer
    std::thread sa_thread;
    std::atomic<bool> spectrum_analyzer_started{false};
    scicpp::signal::Spectrum<float> spectrum;
    PhaseNoiseDensityVector phase_noise;
    MovingAverager<PhaseNoiseDensity> averager;
    CumulativeAverager<ComplexPhaseNoiseDensity> averager_xy;
    std::atomic<bool> reset_cumulative_requested{false};

    // Jitter (integrated noise)
    Phase phase_jitter{0.0f};
    Time time_jitter{0.0f};
    Frequency f_lo_used{0.0f}; // Integration interval start
    Frequency f_hi_used{0.0f}; // Integration interval end

    // Carrier power
    scicpp::units::dimensionless<double> conv_factor_dBm;
    std::array<scicpp::units::electric_potential<double>, 2> vrange;

    // Very-slow FLL used only to keep CORDIC unwrap bounded.
    bool tracking_enabled = true;
    Frequency tracking_bandwidth{0.1f};
    Frequency tracking_max_step{0.05f};
    Frequency tracking_max_correction{100.0f};

    std::array<Frequency, 4> base_dds_freq{
        Frequency{10.0e6f}, Frequency{10.0e6f}, Frequency{10.0e6f}, Frequency{10.0e6f}
    };

    std::array<Frequency, 4> tracking_correction{
        Frequency{0.0f}, Frequency{0.0f}, Frequency{0.0f}, Frequency{0.0f}
    };
    Phase tracking_last_mean_dphi{0.0f};
    Frequency tracking_last_error{0.0f};
    bool tracking_locked = false;
    std::array<TrackingLock, 2> tracking_locks;

    // ----------------- Private functions

    void load_config();
    void reset_phase_unwrapper();
    void set_frequency_scalings();
    void update_interferometer_transfer_function();
    void set_power_conversion_factor();
    auto compute_phase_noise(SpectrumPhaseArray& new_phase);
    auto compute_crossed_phase_noise(SpectrumPhaseArray& new_phase_x, SpectrumPhaseArray& new_phase_y);
    void compute_jitter(Frequency f_dut);
    double carrier_power(uint32_t navg);
    void configure_cic_rate(uint32_t rate);
    void invalidate_acquisition();
    Frequency effective_tracking_bandwidth() const;
    void apply_tracking_update(Phase mean_dphi, Time block_duration, uint32_t input_channel);
    void start_spectrum_analyzer();
    void spectrum_analyzer_thread();
};

#endif // __PHASE_NOISE_ANALYZER_HPP__
