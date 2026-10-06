#pragma once
#include "phase-processing.hpp"
#include "streaming-welch.hpp"
#include "stream-coverage.hpp"
#include "stream-performance.hpp"
#include "spectrum-publication.hpp"
#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/config_manager.hpp"
#include "server/drivers/phase-noise/cyclic-phase-dma.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <optional>
#include <array>
#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <mutex>
#include <thread>
#include <chrono>
#include <tuple>
#include <vector>
#include <scicpp/core.hpp>
#include <scicpp/signal.hpp>

#include "server/runtime/driver_manager.hpp"
#include "server/hardware/memory_manager.hpp"

#include "moving-averager.hpp"
#include "phase-calibration.hpp"
#include "server/drivers/phase-noise/tracking-lock.hpp"

namespace rt { class ConfigManager; }

namespace phase_noise {

template<class Board>
class Core
{
    using Phase = scicpp::units::radian<float>;
    using Time = scicpp::units::time<float>;
    using Frequency = scicpp::units::frequency<float>;
    using PhaseNoiseDensity = scicpp::units::quantity_divide<
                scicpp::units::quantity_multiply<Phase, Phase>,
                Frequency>;

    static constexpr uint32_t fft_size = 32768;
    static constexpr uint32_t data_size = 2 * fft_size;

    using PhaseDataArray = std::array<Phase, data_size>;
    using RawPhaseDataArray = std::array<int32_t, data_size>;
    using PhaseNoiseDensityVector = std::vector<PhaseNoiseDensity>;

  public:
    Core();
    ~Core();

    void save_config();
    void set_local_oscillator(uint32_t channel, double freq_hz);
    void set_cic_rate(uint32_t rate);
    void set_channel(uint32_t chan);
    void set_fft_navg(uint32_t n_avg);
    void set_analyzer_mode(uint32_t mode);
    void set_interferometer_delay(float delay_s);
    void set_tracking_enabled(bool enabled);
    void set_tracking_bandwidth(float bandwidth_hz);
    void set_tracking_max_step(float max_step_hz);
    void set_tracking_max_correction(float max_correction_hz);
    bool set_phase_precision(uint32_t bits);
    auto get_precision_status() {
        std::shared_lock lk(data_mtx);
        return std::tuple{phase_precision, captured_precision, double(phase_conversion_factor.eval()),
            capture_state, accepted_captures, overflow_captures, dma_errors, processing_ms, capture_period_ms};
    }
    auto get_phase_snapshot() const {
        std::shared_lock lk(data_mtx);
        return std::tuple{accepted_captures, captured_precision, phase_conversion_factor,
                          capture_state == Valid, relative_phase_snapshot(phase_raw, snapshot_scale)};
    }

    auto get_spectrum_snapshot() const { return publication.snapshot(); }
    auto get_stream_status() {
        std::shared_lock lk(data_mtx);
        return std::tuple{spectrum.segment_count(), dma.overruns(), fft_size, fft_size / 2, 3u};
    }
    auto get_stream_coverage() const {
        std::shared_lock lk(data_mtx);
        return coverage.status();
    }
    auto get_stream_performance() {
        std::shared_lock lk(data_mtx);
        const auto completed = dma.completed_chunks();
        const double chunk_ms = 1000.0 * CyclicPhaseDma::samples_per_chunk / double(fs.eval());
        return performance.status(completed > consumed_chunks ? (completed - consumed_chunks) * chunk_ms : 0.0,
            (CyclicPhaseDma::ring_chunks - 3 - data_size / CyclicPhaseDma::samples_per_chunk) * chunk_ms,
            double(fs.eval()) / (fft_size / 2));
    }
    auto get_fft_performance() const {
        std::shared_lock lk(data_mtx);
        return performance.fft_status();
    }
    auto get_dma_status() {
        std::shared_lock lk(data_mtx);
        return std::tuple{dma.completed_chunks(), consumed_chunks, dma.generation(), gap_captures};
    }

    auto get_tracking_parameters() {
        std::shared_lock lk(data_mtx);
        return std::tuple{tracking_enabled, tracking_bandwidth,
            effective_tracking_bandwidth(), tracking_max_step, tracking_max_correction,
            base_dds_freq[0], base_dds_freq[1], tracking_correction[0], tracking_correction[1],
            tracking_error[0], tracking_error[1], tracking_locked[0], tracking_locked[1]};
    }

    auto get_parameters() {
        std::shared_lock lk(data_mtx);
        return std::tuple{
            1u + fft_size / 2,
            fs,
            channel,
            cic_rate,
            fft_navg,
            dds.get_dds_freq(0),
            dds.get_dds_freq(1),
            analyzer_mode,
            interferometer_delay,
            board.reference_clock()
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

    PhaseDataArray get_phase() const;
    PhaseNoiseDensityVector get_phase_noise() const;
    auto get_average_status() const {
        std::shared_lock lk(data_mtx);
        return std::tuple{uint32_t(averager.count()), uint32_t(averager.window())};
    }

  private:
    Board board;
    rt::ConfigManager& cfg;
    CyclicPhaseDma dma;
    uint64_t consumed_chunks = 0;
    typename Board::Oscillator& dds;
    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::status>& sts;

    uint32_t channel = 0;
    uint32_t fft_navg = 1;
    uint32_t cic_rate = prm::cic_decimation_rate_default;
    Phase phase_conversion_factor{0.0f}; // Radians per filtered DMA count
    uint32_t dirty_cnt = 0; // guarded by data_mtx
    uint32_t phase_precision = 0, captured_precision = 0;
    enum CaptureState : uint32_t { Settling, Valid, Overrange, DmaError, SampleGap };
    uint32_t capture_state = Settling;
    uint64_t accepted_captures = 0, overflow_captures = 0, dma_errors = 0, gap_captures = 0;
    double processing_ms = 0.0, capture_period_ms = 0.0;
    std::chrono::steady_clock::time_point last_capture_time{};
    Frequency fs_adc, fs;
    Time dma_transfer_duration;

    // Always acquire dma_mtx before data_mtx when both are needed.
    std::mutex dma_mtx; // serializes epoch changes with captured-settings publication
    std::atomic<uint32_t> dma_settings_pending{0}; // give queued setters the next DMA lock
    mutable std::shared_mutex data_mtx; // settings, processing and published results
    SpectrumPublication<PhaseNoiseDensity> publication{1 + fft_size / 2};
    void publish_spectrum(const std::array<double, 4>& acquired_lo);

    // Data acquisition thread
    std::thread acq_thread;
    std::atomic<bool> acquisition_started{false};

    RawPhaseDataArray phase_raw{};
    Phase snapshot_scale{};

    // Spectrum analyzer
    StreamingWelch<fft_size> spectrum;
    StreamCoverage coverage;
    StreamPerformance performance;
    std::atomic<bool> stream_initialized{false};
    uint64_t estimator_generation = UINT64_MAX;
    PhaseNoiseDensityVector phase_noise;
    PhaseNoiseDensityVector native_phase_noise;
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
    std::array<float, 1 + fft_size / 2> interferometer_tf{}; // Interferometer transfer function

    // Carrier power
    scicpp::units::dimensionless<double> conv_factor_dBm;

    // Slow frequency tracking is opt-in; all frequencies here are double Hz.
    bool tracking_enabled = false;
    double tracking_bandwidth = 0.1;
    double tracking_max_step = 0.05;
    double tracking_max_correction = 100.0;
    std::array<double, 2> base_dds_freq{};
    std::array<double, 2> tracking_correction{};
    std::array<double, 2> tracking_error{};
    std::array<bool, 2> tracking_locked{};
    std::array<TrackingLock, 2> tracking_locks;
    std::array<std::chrono::steady_clock::time_point, 2> tracking_last_update{};

    // ----------------- Private functions

    void load_config();
    void invalidate_results(CaptureState state = Settling); // caller holds data_mtx
    double carrier_power(uint32_t navg); // caller holds data_mtx
    double effective_tracking_bandwidth() const;
    void reset_tracking_observations(); // caller holds data_mtx
    void update_tracking(double slope_radians_per_sample); // caller holds both mutexes when enabled
    void update_interferometer_transfer_function();
    void set_power_conversion_factor();
    void compute_phase_noise(const RawPhaseDataArray& raw, bool seed, double& average_ms);
    auto compute_jitter(const PhaseNoiseDensityVector& new_pn, Frequency acquired_lo);
    void acquisition_thread();
    void start_acquisition();
};


namespace sci = scicpp;
namespace sig = scicpp::signal;

namespace detail {
class DmaSettingsGuard {
    std::atomic<uint32_t>& pending;
  public:
    explicit DmaSettingsGuard(std::atomic<uint32_t>& value) : pending(value) {
        pending.fetch_add(1, std::memory_order_acq_rel);
    }
    ~DmaSettingsGuard() {
        pending.fetch_sub(1, std::memory_order_acq_rel);
        pending.notify_all();
    }
};
}

template<class Board>
Core<Board>::Core()
: cfg    (services::require<rt::ConfigManager>())
, dds    (rt::get_driver<typename Board::Oscillator>())
, ctl    (hw::get_memory<mem::control>())
, sts    (hw::get_memory<mem::status>())
, phase_noise(1 + fft_size / 2)
, averager(1)
{
    fs_adc = Frequency(board.sampling_frequency());

    ctl.write_mask<reg::cordic, 0b11>(0b11); // Phase accumulator on

    load_config();

    phase_noise.reserve(1 + fft_size / 2);
    start_acquisition();
}

template<class Board>
Core<Board>::~Core() {
    acquisition_started.store(false, std::memory_order_release);
    if (acq_thread.joinable()) {
        acq_thread.join();
    }
}

template<class Board>
void Core<Board>::save_config() {
    std::unique_lock lk(data_mtx);
    cfg.set("PhaseNoiseAnalyzer", "channel", channel);
    cfg.set("PhaseNoiseAnalyzer", "fft_navg", fft_navg);
    cfg.set("PhaseNoiseAnalyzer", "cic_rate", cic_rate);
    if constexpr (Board::max_phase_precision > 0)
        cfg.set("PhaseNoiseAnalyzer", "phase_precision", phase_precision);
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[0]", base_dds_freq[0]);
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[1]", base_dds_freq[1]);
    cfg.set("PhaseNoiseAnalyzer", "analyzer_mode", analyzer_mode);
    cfg.set("PhaseNoiseAnalyzer", "interferometer_delay", interferometer_delay.eval());
    cfg.set("PhaseNoiseAnalyzer", "tracking_enabled", tracking_enabled);
    cfg.set("PhaseNoiseAnalyzer", "tracking_bandwidth", tracking_bandwidth);
    cfg.set("PhaseNoiseAnalyzer", "tracking_max_step", tracking_max_step);
    cfg.set("PhaseNoiseAnalyzer", "tracking_max_correction", tracking_max_correction);
    cfg.save();
}

template<class Board>
void Core<Board>::set_local_oscillator(uint32_t lo_channel, double freq_hz) {
    detail::DmaSettingsGuard pending(dma_settings_pending);
    std::unique_lock dma_lk(dma_mtx);
    std::unique_lock lk(data_mtx);
    if (lo_channel >= 2 || !std::isfinite(freq_hz) || freq_hz < 0.0 ||
        freq_hz > static_cast<double>(fs_adc.eval()) / 2.0) {
        log<ERROR>("PhaseNoiseAnalyzer: Invalid local oscillator setting\n");
        return;
    }
    // Compare hardware tuning words: a read-back frequency (or another value
    // rounding to it) should preserve acquisition. A manual request must still
    // restore the nominal LO when tracking has applied a correction.
    const double tuning_factor = std::ldexp(1.0, 48) / board.sampling_frequency();
    if (acquisition_started.load(std::memory_order_acquire) &&
        std::llround(freq_hz * tuning_factor) == std::llround(base_dds_freq[lo_channel] * tuning_factor) &&
        std::llround(freq_hz * tuning_factor) == std::llround(dds.get_dds_freq(lo_channel) * tuning_factor))
        return;
    dma.configure_sampling(fs, [&] { dds.set_dds_freq(lo_channel, freq_hz); });
    base_dds_freq[lo_channel] = dds.get_dds_freq(lo_channel);
    tracking_correction[lo_channel] = 0.0;
    set_power_conversion_factor();
    dirty_cnt = 4;
    invalidate_results();
}

template<class Board>
void Core<Board>::set_cic_rate(uint32_t rate) {
    if (rate < prm::cic_decimation_rate_min ||
        rate > prm::cic_decimation_rate_max) {
        log<ERROR>("PhaseNoiseAnalyzer: CIC rate out of range\n");
        return;
    }

    detail::DmaSettingsGuard pending(dma_settings_pending);
    std::unique_lock dma_lk(dma_mtx);
    std::unique_lock lk(data_mtx);

    // Construction must still initialize the rate and calibration, even when
    // the saved setting equals the member's default.
    if (acquisition_started.load(std::memory_order_acquire) && rate == cic_rate) return;
    cic_rate = rate;
    phase_conversion_factor = float(phase_calibration::filter_correction(
        rate, prm::cic_n_stages, prm::cic_differential_delay) * std::exp2(-double(phase_precision))) * sci::pi<Phase> / 8192.0f;
    fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
    dma_transfer_duration = data_size / fs;
    logf("Spectrum window duration = {} s\n", dma_transfer_duration.eval());

    update_interferometer_transfer_function();
    invalidate_results();
    dirty_cnt = 2;
    dma.configure_sampling(fs, [&] { ctl.write<reg::cic_rate>(cic_rate); });
}

template<class Board>
bool Core<Board>::set_phase_precision(uint32_t bits) {
    if (bits > Board::max_phase_precision) return false;
    detail::DmaSettingsGuard pending(dma_settings_pending);
    std::unique_lock dma_lk(dma_mtx);
    std::unique_lock lk(data_mtx);
    if (acquisition_started.load(std::memory_order_acquire) && bits == phase_precision) return true;
    // Restart the complete hardware epoch, including queued FIFO samples.
    dma.configure_sampling(fs, [&] {
        if constexpr (Board::max_phase_precision > 0) board.set_phase_precision(bits);
    });
    phase_precision = bits;
    phase_conversion_factor = float(phase_calibration::filter_correction(
        cic_rate, prm::cic_n_stages, prm::cic_differential_delay) * std::exp2(-double(bits))) * sci::pi<Phase> / 8192.0f;
    dirty_cnt = std::max(dirty_cnt, 2u);
    invalidate_results();
    return true;
}

template<class Board>
void Core<Board>::set_channel(uint32_t chan) {
    if (chan != 0 && chan != 1) {
        log<ERROR>("PhaseNoiseAnalyzer: Invalid channel\n");
        return;
    }

    detail::DmaSettingsGuard pending(dma_settings_pending);
    std::unique_lock dma_lk(dma_mtx);
    std::unique_lock lk(data_mtx);
    if (acquisition_started.load(std::memory_order_acquire) && chan == channel) return;
    channel = chan;
    dirty_cnt = 2;
    invalidate_results();
    set_power_conversion_factor();
    dma.configure_sampling(fs, [&] {
        ctl.write_mask<reg::cordic, 0b10000>((channel & 1) << 4);
    });
}

// Carrier power in dBm
template<class Board>
double Core<Board>::get_carrier_power(uint32_t navg) {
    std::shared_lock lk(data_mtx);
    return carrier_power(navg);
}

template<class Board>
double Core<Board>::carrier_power(uint32_t navg) {
    if (navg == 0) return std::numeric_limits<double>::quiet_NaN();
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

    return 10.0 * sci::log10(conv_factor_dBm * res / double(navg));
}

template<class Board>
typename Core<Board>::PhaseDataArray Core<Board>::get_phase() const {
    std::shared_lock lk(data_mtx);
    return relative_phase_snapshot(phase_raw, snapshot_scale);
}

template<class Board>
typename Core<Board>::PhaseNoiseDensityVector Core<Board>::get_phase_noise() const {
    return publication.spectrum();
}

template<class Board>
void Core<Board>::set_fft_navg(uint32_t n_avg) {
    std::unique_lock lk(data_mtx);
    const auto target = std::clamp(n_avg, 1u, 100u);
    if (fft_navg == target) return;
    fft_navg = target;
    averager.set_navg(fft_navg);
    if (capture_state == Valid) {
        averager.average_to(native_phase_noise);
        spectrum.order_to(native_phase_noise, phase_noise);
        const auto frequencies = publication.settings().lo;
        compute_jitter(phase_noise, Frequency(frequencies[channel]));
        publish_spectrum(frequencies);
    }
}

template<class Board>
void Core<Board>::set_analyzer_mode(uint32_t mode) {
    if (mode != AnalyzerMode::RF && mode != AnalyzerMode::LASER) {
        logf<WARNING>("PhaseNoiseAnalyzer: Invalid mode {}\n", mode);
        return;
    }

    std::unique_lock lk(data_mtx);
    if (analyzer_mode != mode) {
        analyzer_mode = mode;
        invalidate_results();
    }
}

template<class Board>
void Core<Board>::set_interferometer_delay(float delay_s) {
    if (!std::isfinite(delay_s) || delay_s < 0.0f) {
        log<ERROR>("PhaseNoiseAnalyzer: Invalid interferometer delay\n");
        return;
    }
    std::unique_lock lk(data_mtx);
    interferometer_delay = Time(delay_s);
    logf("PhaseNoiseAnalyzer: Interferometer delay set to {} ns\n",
         interferometer_delay.eval() * 1E9f);

    update_interferometer_transfer_function();
    invalidate_results();
}

template<class Board>
void Core<Board>::set_tracking_enabled(bool enabled) {
    // Disabling restores nominal frequencies and discards settling captures.
    detail::DmaSettingsGuard pending(dma_settings_pending);
    std::unique_lock dma_lk(dma_mtx);
    std::unique_lock lk(data_mtx);
    if (tracking_enabled == enabled) return;
    tracking_enabled = enabled;
    if (!enabled) {
        dma.configure_sampling(fs, [&] {
            for (uint32_t i = 0; i < 2; ++i) {
                dds.set_dds_freq(i, base_dds_freq[i]);
                tracking_correction[i] = 0.0;
            }
        });
        set_power_conversion_factor();
    }
    dirty_cnt = std::max(dirty_cnt, 4u);
    invalidate_results();
}

template<class Board>
void Core<Board>::set_tracking_bandwidth(float bandwidth_hz) {
    if (!std::isfinite(bandwidth_hz) || bandwidth_hz < 0.0f) {
        log<ERROR>("PhaseNoiseAnalyzer: Invalid tracking bandwidth\n");
        return;
    }
    std::unique_lock lk(data_mtx);
    tracking_bandwidth = static_cast<double>(bandwidth_hz);
    reset_tracking_observations();
}

template<class Board>
void Core<Board>::set_tracking_max_step(float max_step_hz) {
    if (!std::isfinite(max_step_hz) || max_step_hz < 0.0f) {
        log<ERROR>("PhaseNoiseAnalyzer: Invalid tracking step limit\n");
        return;
    }
    std::unique_lock lk(data_mtx);
    tracking_max_step = static_cast<double>(max_step_hz);
    reset_tracking_observations();
}

template<class Board>
void Core<Board>::set_tracking_max_correction(float max_correction_hz) {
    if (!std::isfinite(max_correction_hz) || max_correction_hz < 0.0f) {
        log<ERROR>("PhaseNoiseAnalyzer: Invalid tracking correction limit\n");
        return;
    }
    detail::DmaSettingsGuard pending(dma_settings_pending);
    std::unique_lock dma_lk(dma_mtx);
    std::unique_lock lk(data_mtx);
    tracking_max_correction = static_cast<double>(max_correction_hz);
    // Apply a tighter bound immediately, treating it as a manual retune.
    bool retuned = false;
    for (uint32_t i = 0; i < 2; ++i) {
        if (std::abs(tracking_correction[i]) > tracking_max_correction) {
            const double correction = std::clamp(tracking_correction[i],
                -tracking_max_correction, tracking_max_correction);
            dma.configure_sampling(fs, [&] { dds.set_dds_freq(i, base_dds_freq[i] + correction); });
            tracking_correction[i] = dds.get_dds_freq(i) - base_dds_freq[i];
            retuned = true;
        }
    }
    if (retuned) {
        set_power_conversion_factor();
        dirty_cnt = std::max(dirty_cnt, 4u);
        invalidate_results();
    } else {
        reset_tracking_observations();
    }
}

// ----------------- Private functions

template<class Board>
void Core<Board>::load_config() {
    if (cfg.has("PhaseNoiseAnalyzer", "channel")) {
        set_channel(cfg.get<uint32_t>("PhaseNoiseAnalyzer", "channel"));
    } else {
        set_channel(0);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "fft_navg")) {
        set_fft_navg(cfg.get<uint32_t>("PhaseNoiseAnalyzer", "fft_navg"));
    } else {
        set_fft_navg(1);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "cic_rate")) {
        set_cic_rate(cfg.get<uint32_t>("PhaseNoiseAnalyzer", "cic_rate"));
    } else {
        set_cic_rate(prm::cic_decimation_rate_default);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[0]")) {
        set_local_oscillator(0, cfg.get<double>("PhaseNoiseAnalyzer", "dds_freq[0]"));
    } else {
        set_local_oscillator(0, 10E6);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[1]")) {
        set_local_oscillator(1, cfg.get<double>("PhaseNoiseAnalyzer", "dds_freq[1]"));
    } else {
        set_local_oscillator(1, 10E6);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "analyzer_mode")) {
        set_analyzer_mode(cfg.get<uint32_t>("PhaseNoiseAnalyzer", "analyzer_mode"));
    } else {
        set_analyzer_mode(AnalyzerMode::RF);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "interferometer_delay")) {
        set_interferometer_delay(cfg.get<float>("PhaseNoiseAnalyzer", "interferometer_delay"));
    } else {
        set_interferometer_delay(0.0f);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "tracking_bandwidth"))
        set_tracking_bandwidth(cfg.get<float>("PhaseNoiseAnalyzer", "tracking_bandwidth"));
    if (cfg.has("PhaseNoiseAnalyzer", "tracking_max_step"))
        set_tracking_max_step(cfg.get<float>("PhaseNoiseAnalyzer", "tracking_max_step"));
    if (cfg.has("PhaseNoiseAnalyzer", "tracking_max_correction"))
        set_tracking_max_correction(cfg.get<float>("PhaseNoiseAnalyzer", "tracking_max_correction"));
    if (cfg.has("PhaseNoiseAnalyzer", "tracking_enabled"))
        set_tracking_enabled(cfg.get<bool>("PhaseNoiseAnalyzer", "tracking_enabled"));
    if constexpr (Board::max_phase_precision > 0) {
        const auto bits = cfg.has("PhaseNoiseAnalyzer", "phase_precision")
            ? cfg.get<uint32_t>("PhaseNoiseAnalyzer", "phase_precision") : 0u;
        if (!set_phase_precision(bits)) set_phase_precision(0);
    }
}

template<class Board>
void Core<Board>::update_interferometer_transfer_function() {
    using namespace sci::operators;
    auto freqs = sig::rfftfreq<fft_size>(1.0f / fs);
    interferometer_tf = std::move(sci::pow<2>(
        0.5f / sci::sin(sci::pi<Phase> * std::move(freqs) * interferometer_delay)));
}

template<class Board>
void Core<Board>::set_power_conversion_factor() {
    conv_factor_dBm = sci::units::dimensionless<double>(
        board.power_conversion(channel, dds.get_dds_freq(channel)));
}

template<class Board>
void Core<Board>::compute_phase_noise(const RawPhaseDataArray& raw, bool seed, double& average_ms) {
    const std::size_t first = seed ? 0 : data_size - fft_size;
    if (seed) spectrum.reset();
    for (std::size_t offset = first; offset + fft_size <= data_size; offset += fft_size / 2)
        spectrum.process(std::span<const int32_t>(raw.data() + offset, fft_size),
                         phase_conversion_factor.eval(), fs.eval(), {}, 0, true, true);
    const auto& density = spectrum.density();
    const auto average_start = StreamClock::now();
    // Keep one-window history current even while averaging is disabled.
    averager.append_transformed(density, [this](std::size_t k, float value) {
        return PhaseNoiseDensity{value * (analyzer_mode == AnalyzerMode::LASER ? interferometer_tf[spectrum.frequency_bin(k)] : 1.f)};
    });
    average_ms = elapsed_ms(average_start);
}

template<class Board>
auto Core<Board>::compute_jitter(const PhaseNoiseDensityVector& new_pn, Frequency acquired_lo) {
    const auto f_dss = acquired_lo;

    if (sci::almost_equal(f_dss, Frequency{0.0f})) {
        // No demodulation if DSS frequency is zero
        phase_jitter = std::numeric_limits<Phase>::quiet_NaN();
        time_jitter  = std::numeric_limits<Time>::quiet_NaN();
        f_lo_used    = std::numeric_limits<Frequency>::quiet_NaN();
        f_hi_used    = std::numeric_limits<Frequency>::quiet_NaN();
    } else {
        const std::size_t n_bins = new_pn.size();
        const auto df = fs / float(fft_size);
        const auto f_min_avail = df;
        const auto f_max_avail = (n_bins - 1) * df;

        auto log10f = [](Frequency f) {
            const auto fmin = std::numeric_limits<Frequency>::min();
            return std::log10(sci::units::fmax(f, fmin).eval());
        };

        auto pow10f = [](float x) {
            return Frequency(std::pow(10.0f, x));
        };

        const float low_dec  = std::ceil(log10f(f_min_avail));
        const float high_dec = std::floor(log10f(f_max_avail));

        if (high_dec <= low_dec) {
            // No full decade: integrate whole available band (excluding DC)
            f_lo_used = f_min_avail;
            f_hi_used = f_max_avail;
            phase_jitter = sci::sqrt(sci::trapz(new_pn.begin() + 1, new_pn.end(), df));
        } else {
            f_lo_used = pow10f(low_dec);
            f_hi_used = pow10f(high_dec);

            std::size_t k1 = std::max(std::size_t{1}, static_cast<std::size_t>(sci::ceil(f_lo_used / df).eval()));
            std::size_t k2 = std::min(n_bins - 1u, static_cast<std::size_t>(sci::floor(f_hi_used / df).eval()));

            if (k2 <= k1) {
                k1 = std::max(std::size_t{1}, k1);
                k2 = std::min(n_bins - 1u, std::max(k1 + 1, k2));
            }

            phase_jitter = sci::sqrt(sci::trapz(new_pn.begin() + k1, new_pn.begin() + k2 + 1, df));
        }

        time_jitter = phase_jitter / (2.0f * sci::pi<Phase> * f_dss);
    }
}

template<class Board>
void Core<Board>::start_acquisition() {
    bool expected = false;
    if (acquisition_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        acq_thread = std::thread{&Core<Board>::acquisition_thread, this};
    }
}

template<class Board>
void Core<Board>::publish_spectrum(const std::array<double, 4>& acquired_lo) {
    SpectrumMetadata metadata;
    metadata.state = capture_state;
    metadata.precision = captured_precision;
    metadata.fs = double(fs.eval());
    metadata.channel = channel;
    metadata.cic_rate = cic_rate;
    metadata.navg = fft_navg;
    metadata.count = averager.count();
    metadata.target = averager.window();
    metadata.lo = acquired_lo;
    metadata.mode = analyzer_mode;
    metadata.delay = double(interferometer_delay.eval());
    metadata.reference_clock = board.reference_clock();
    publication.publish(metadata, phase_noise);
}

template<class Board>
void Core<Board>::invalidate_results(CaptureState state) {
    coverage.reset();
    performance.reset();
    stream_initialized.store(false);
    estimator_generation = UINT64_MAX;
    capture_state = state;
    reset_tracking_observations();
    averager.clear();
    phase_raw.fill(0);
    phase_noise.assign(1 + fft_size / 2, PhaseNoiseDensity{});
    publish_spectrum({dds.get_dds_freq(0), dds.get_dds_freq(1), 0.0, 0.0});
    phase_jitter = std::numeric_limits<Phase>::quiet_NaN();
    time_jitter = std::numeric_limits<Time>::quiet_NaN();
    f_lo_used = std::numeric_limits<Frequency>::quiet_NaN();
    f_hi_used = std::numeric_limits<Frequency>::quiet_NaN();
}

template<class Board>
double Core<Board>::effective_tracking_bandwidth() const {
    // Keep the loop at least 100 times below the first displayed offset (2 bins).
    return std::min({tracking_bandwidth, 0.1,
                     static_cast<double>(fs.eval()) / (50.0 * fft_size)});
}

template<class Board>
void Core<Board>::reset_tracking_observations() {
    tracking_locks = {};
    tracking_locked.fill(false);
    tracking_error.fill(std::numeric_limits<double>::quiet_NaN());
    tracking_last_update = {};
}

template<class Board>
void Core<Board>::update_tracking(double slope_radians_per_sample) {
    const double error = slope_radians_per_sample *
                         static_cast<double>(fs.eval()) / (2.0 * sci::pi<double>);
    tracking_error[channel] = error;
    const double bandwidth = effective_tracking_bandwidth();
    if (!tracking_enabled || bandwidth <= 0.0 || tracking_max_step <= 0.0 ||
        tracking_max_correction <= 0.0 || base_dds_freq[channel] <= 0.0 || !std::isfinite(error)) {
        tracking_locked[channel] = false;
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    double elapsed = static_cast<double>(dma_transfer_duration.eval());
    if (tracking_last_update[channel] != std::chrono::steady_clock::time_point{})
        elapsed = std::max(elapsed, std::chrono::duration<double>(now - tracking_last_update[channel]).count());
    tracking_last_update[channel] = now;
    const double alpha = std::min(-std::expm1(-2.0 * sci::pi<double> * bandwidth * elapsed), 0.2);
    const double step = std::clamp(-alpha * error, -tracking_max_step, tracking_max_step);
    const double correction = std::clamp(tracking_correction[channel] + step,
                                          -tracking_max_correction, tracking_max_correction);
    const double requested = std::clamp(base_dds_freq[channel] + correction,
        0.0, static_cast<double>(fs_adc.eval()) / 2.0);
    // The mixer uses cos(LO) + j*sin(LO): measured slope is LO minus input.
    // Retune only after copying a coherent window; reset queued history so
    // the next window cannot straddle the DDS change.
    if (std::abs(requested - dds.get_dds_freq(channel)) >=
        board.sampling_frequency() / std::pow(2.0, 49)) {
        dma.configure_sampling(fs, [&] { dds.set_dds_freq(channel, requested); });
        tracking_correction[channel] = dds.get_dds_freq(channel) - base_dds_freq[channel];
        set_power_conversion_factor();
    }
    const bool saturated = std::abs(correction) >= tracking_max_correction ||
        requested <= 0.0 || requested >= static_cast<double>(fs_adc.eval()) / 2.0;
    tracking_locked[channel] = tracking_locks[channel].update(error, alpha, 0.1 * tracking_max_step) && !saturated;
}

template<class Board>
void Core<Board>::acquisition_thread() {
    {
        std::unique_lock dma_lk(dma_mtx);
        dma.start_acquisition();
    }
    while (acquisition_started.load(std::memory_order_acquire)) {
        auto pending = dma_settings_pending.load(std::memory_order_acquire);
        while (pending > 0) {
            dma_settings_pending.wait(pending, std::memory_order_acquire);
            pending = dma_settings_pending.load(std::memory_order_acquire);
        }
        if (!acquisition_started.load(std::memory_order_acquire)) break;
        // Polling never holds the settings/processing locks. A slow window at
        // high decimation is cancellable and does not delay a rate/LO request.
        auto snapshot = dma.read<data_size>(consumed_chunks, acquisition_started,
            (fft_size / 2) / CyclicPhaseDma::samples_per_chunk, stream_initialized.load());
        std::unique_lock dma_lk(dma_mtx);
        std::unique_lock lk(data_mtx);
        if (!acquisition_started.load(std::memory_order_acquire)) break;
        // A setter may have restarted the producer after the copy completed.
        if (snapshot && snapshot->generation != dma.generation()) continue;
        const auto now = std::chrono::steady_clock::now();
        if (last_capture_time != std::chrono::steady_clock::time_point{})
            capture_period_ms = std::chrono::duration<double, std::milli>(now - last_capture_time).count();
        last_capture_time = now;
        if (snapshot) {
            consumed_chunks = snapshot->end_chunk;
            stream_initialized.store(true);
            captured_precision = snapshot->precision;
        }
        if (!snapshot) {
            ++dma_errors;
            dirty_cnt = std::max(dirty_cnt, 2u);
            invalidate_results(DmaError);
            dma.configure_sampling(fs, [] {});
            dma.start_acquisition();
        } else if (snapshot->sample_gap || snapshot->overflow) {
            if (snapshot->overflow) ++overflow_captures;
            if (snapshot->sample_gap) ++gap_captures;
            invalidate_results(snapshot->sample_gap ? SampleGap : Overrange);
            // Gap/overflow is sticky for the hardware epoch. Restart it before
            // taking another window rather than accepting damaged history.
            dma.configure_sampling(fs, [] {});
        } else if (!snapshot->matches_precision(phase_precision)) {
            invalidate_results();
            dma.configure_sampling(fs, [] {});
        } else if (dirty_cnt > 0) {
            --dirty_cnt;
            capture_state = Settling;
        } else {
            const auto process_start = std::chrono::steady_clock::now();
            const std::array<double, 4> acquired_frequencies{
                dds.get_dds_freq(0), dds.get_dds_freq(1), 0.0, 0.0};
            const auto acquired_lo = Frequency(acquired_frequencies[channel]);
            const auto trend = fit_raw_phase_prefix<data_size>(snapshot->samples);
            update_tracking(trend.slope * double(phase_conversion_factor.eval()));
            dma_lk.unlock();
            const bool seed = estimator_generation != snapshot->generation || snapshot->skipped_hops;
            estimator_generation = snapshot->generation;
            const auto fft_start = StreamClock::now();
            double average_ms = 0;
            compute_phase_noise(snapshot->samples, seed, average_ms);
            const double fft_ms = elapsed_ms(fft_start) - average_ms;
            double publication_ms = 0;
            phase_raw = std::move(snapshot->samples);
            snapshot_scale = phase_conversion_factor;
            coverage.append(snapshot->end_chunk,
                (seed ? data_size : fft_size) / CyclicPhaseDma::samples_per_chunk);
            ++accepted_captures;
            capture_state = Valid;
            if (publication.ready()) {
                const auto publish_start = StreamClock::now();
                averager.average_to(native_phase_noise);
                spectrum.order_to(native_phase_noise, phase_noise);
                compute_jitter(phase_noise, acquired_lo);
                publish_spectrum(acquired_frequencies);
                publication_ms = elapsed_ms(publish_start);
            }
            processing_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - process_start).count();
            performance.append(processing_ms, fft_ms, average_ms, publication_ms, snapshot->copy_ms, spectrum.stage_times());
        }
        if (!snapshot) {
            lk.unlock();
            dma_lk.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

} // namespace phase_noise
