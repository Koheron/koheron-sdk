#pragma once
#include "phase-processing.hpp"
#include "welch-spectrum.hpp"
#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/config_manager.hpp"
#include "server/drivers/dma-s2mm.hpp"
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
class DmaS2MM;

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
    // Memory offsets are bytes; take the middle of the packet after settling.
    static constexpr uint32_t read_offset = ((prm::n_pts - data_size) / 2) * sizeof(int32_t);

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
                          capture_state == Valid, phase};
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
    DmaS2MM& dma;
    typename Board::Oscillator& dds;
    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::status>& sts;

    uint32_t channel = 0;
    uint32_t fft_navg = 1;
    uint32_t cic_rate = prm::cic_decimation_rate_default;
    Phase phase_conversion_factor{0.0f}; // Radians per filtered DMA count
    uint32_t dirty_cnt = 0; // guarded by data_mtx
    uint32_t phase_precision = 0, captured_precision = 0;
    enum CaptureState : uint32_t { Settling, Valid, Overrange, DmaError };
    uint32_t capture_state = Settling;
    uint64_t accepted_captures = 0, overflow_captures = 0, dma_errors = 0;
    double processing_ms = 0.0, capture_period_ms = 0.0;
    std::chrono::steady_clock::time_point last_capture_time{};
    Frequency fs_adc, fs;
    Time dma_transfer_duration;

    // Always acquire dma_mtx before data_mtx when both are needed.
    std::mutex dma_mtx; // serializes DMA operations and CIC rate changes
    std::atomic<uint32_t> dma_settings_pending{0}; // give queued setters the next DMA lock
    mutable std::shared_mutex data_mtx; // settings, processing and published results
    // The published spectrum can be read while the next FFT is processing.
    // Writers hold data_mtx before spectrum_mtx; readers only take spectrum_mtx.
    mutable std::shared_mutex spectrum_mtx;

    // Data acquisition thread
    std::thread acq_thread;
    std::atomic<bool> acquisition_started{false};

    PhaseDataArray phase{};

    // Spectrum analyzer
    WelchSpectrum<fft_size> spectrum;
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
    void invalidate_results(); // caller holds data_mtx
    double carrier_power(uint32_t navg); // caller holds data_mtx
    double effective_tracking_bandwidth() const;
    void reset_tracking_observations(); // caller holds data_mtx
    void update_tracking(double slope_radians_per_sample); // caller holds both mutexes when enabled
    void reset_phase_unwrapper();
    // Caller must hold dma_mtx for DMA operations.
    void kick_dma();
    auto read_dma();
    void update_interferometer_transfer_function();
    void set_power_conversion_factor();
    auto compute_phase_noise(const RawPhaseDataArray& raw, const RawPhaseTrend& trend,
                             PhaseDataArray& phase_snapshot);
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
, dma    (rt::get_driver<DmaS2MM>())
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
    std::unique_lock lk(data_mtx);
    if (lo_channel >= 2 || !std::isfinite(freq_hz) || freq_hz < 0.0 ||
        freq_hz > static_cast<double>(fs_adc.eval()) / 2.0) {
        log<ERROR>("PhaseNoiseAnalyzer: Invalid local oscillator setting\n");
        return;
    }
    dds.set_dds_freq(lo_channel, freq_hz);
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
    std::unique_lock dma_lk(dma_mtx); // block until any DMA transfer finishes
    if (acquisition_started.load(std::memory_order_acquire)) {
        Time duration;
        bool changing;
        {
            std::shared_lock lk(data_mtx);
            duration = dma_transfer_duration;
            changing = rate != cic_rate;
        }
        // Processing may already have started the next transfer. Finish it at
        // its original rate before changing the expected DMA duration.
        if (changing) (void)dma.wait_for_transfer_checked(duration);
    }
    std::unique_lock lk(data_mtx);

    cic_rate = rate;
    phase_conversion_factor = float(phase_calibration::filter_correction(
        rate, prm::cic_n_stages, prm::cic_differential_delay) * std::exp2(-double(phase_precision))) * sci::pi<Phase> / 8192.0f;
    fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
    dma_transfer_duration = prm::n_pts / fs;
    logf("DMA transfer duration = {} s\n", dma_transfer_duration.eval());

    update_interferometer_transfer_function();
    invalidate_results();
    dirty_cnt = 2;
    ctl.write<reg::cic_rate>(cic_rate);
}

template<class Board>
bool Core<Board>::set_phase_precision(uint32_t bits) {
    if (bits > Board::max_phase_precision) return false;
    std::unique_lock lk(data_mtx);
    // The FPGA latches this at packet start. Packet metadata lets acquisition
    // reject the old scale, so a precision request need not wait for slow DMA.
    if constexpr (Board::max_phase_precision > 0) board.set_phase_precision(bits);
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

    std::unique_lock lk(data_mtx);
    channel = chan;
    dirty_cnt = 2;
    invalidate_results();
    set_power_conversion_factor();
    ctl.write_mask<reg::cordic, 0b10000>((channel & 1) << 4);
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
    return phase;
}

template<class Board>
typename Core<Board>::PhaseNoiseDensityVector Core<Board>::get_phase_noise() const {
    std::shared_lock lk(spectrum_mtx);
    return phase_noise;
}

template<class Board>
void Core<Board>::set_fft_navg(uint32_t n_avg) {
    std::unique_lock lk(data_mtx);
    fft_navg = std::clamp(n_avg, 1u, 100u);
    averager.set_navg(fft_navg);
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
        for (uint32_t i = 0; i < 2; ++i) {
            dds.set_dds_freq(i, base_dds_freq[i]);
            tracking_correction[i] = 0.0;
        }
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
            dds.set_dds_freq(i, base_dds_freq[i] + correction);
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
void Core<Board>::reset_phase_unwrapper() {
    ctl.write_mask<reg::cordic, 0b1100>(0b1100);
    ctl.write_mask<reg::cordic, 0b1100>(0b0000);
}

template<class Board>
void Core<Board>::kick_dma() {
    reset_phase_unwrapper();
    dma.start_transfer<mem::ram, prm::n_pts, int32_t>();
}

template<class Board>
auto Core<Board>::read_dma() {
    std::optional<std::array<int32_t, data_size>> samples;
    Time duration;
    {
        std::shared_lock lk(data_mtx);
        duration = dma_transfer_duration;
    }
    if (!dma.wait_for_transfer_checked(duration)) return samples;
    auto& ram = hw::get_memory<mem::ram>();
    samples = ram.read_array<int32_t, data_size, read_offset>();
    return samples;
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
auto Core<Board>::compute_phase_noise(const RawPhaseDataArray& raw, const RawPhaseTrend& trend,
                                    PhaseDataArray& phase_snapshot) {
    auto phase_psd = spectrum.density(raw, trend, phase_conversion_factor, fs, &phase_snapshot);

    if (analyzer_mode == AnalyzerMode::LASER) {
        using namespace sci::operators;
        phase_psd = std::move(phase_psd) * interferometer_tf;
    }

    // Keep one-window history current even while averaging is disabled.
    averager.append(std::move(phase_psd));
    return averager.average();
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
void Core<Board>::invalidate_results() {
    capture_state = Settling;
    reset_tracking_observations();
    averager.clear();
    phase.fill(Phase{});
    {
        std::unique_lock spectrum_lk(spectrum_mtx);
        phase_noise.assign(1 + fft_size / 2, PhaseNoiseDensity{});
    }
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
    // Change the selected LO after the drift fit and before the next packet.
    if (std::abs(requested - dds.get_dds_freq(channel)) >=
        board.sampling_frequency() / std::pow(2.0, 49)) {
        dds.set_dds_freq(channel, requested);
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
        std::unique_lock lk(data_mtx);
        kick_dma();
    }

    while (acquisition_started.load(std::memory_order_acquire)) {
        auto pending = dma_settings_pending.load(std::memory_order_acquire);
        while (pending > 0) {
            dma_settings_pending.wait(pending, std::memory_order_acquire);
            pending = dma_settings_pending.load(std::memory_order_acquire);
        }
        if (!acquisition_started.load(std::memory_order_acquire)) break;
        std::unique_lock dma_lk(dma_mtx);
        auto samples = read_dma(); // wait without blocking snapshot getters
        uint32_t packet_status = 0;
        if constexpr (Board::max_phase_precision > 0)
            packet_status = board.phase_packet_status();
        std::unique_lock lk(data_mtx);
        if (!acquisition_started.load(std::memory_order_acquire)) break;
        const auto now = std::chrono::steady_clock::now();
        if (last_capture_time != std::chrono::steady_clock::time_point{})
            capture_period_ms = std::chrono::duration<double, std::milli>(now - last_capture_time).count();
        last_capture_time = now;
        captured_precision = packet_status & 0xfu;

        // Tracking must update the LO between packets. Do that before spectral
        // processing so the next transfer can run while the FFT is computed.
        const bool tracking_capture = tracking_enabled;
        if (!tracking_capture) {
            kick_dma();
            dma_lk.unlock();
        }

        if (!samples) {
            ++dma_errors;
            dirty_cnt = std::max(dirty_cnt, 2u);
            invalidate_results();
            capture_state = DmaError;
        } else if (packet_status & 0x10u) {
            ++overflow_captures;
            invalidate_results();
            capture_state = Overrange;
        } else if (captured_precision != phase_precision) {
            invalidate_results();
        } else if (dirty_cnt > 0) {
            --dirty_cnt;
            capture_state = Settling;
        } else {
            const auto process_start = std::chrono::steady_clock::now();
            const auto acquired_lo = Frequency(dds.get_dds_freq(channel));
            const auto trend = fit_raw_phase_prefix<data_size>(*samples);
            update_tracking(trend.slope * double(phase_conversion_factor.eval()));
            if (tracking_capture) {
                kick_dma();
                dma_lk.unlock();
            }
            PhaseDataArray new_phase{};
            auto new_pn = compute_phase_noise(*samples, trend, new_phase);
            compute_jitter(new_pn, acquired_lo);
            phase = std::move(new_phase);
            {
                std::unique_lock spectrum_lk(spectrum_mtx);
                phase_noise = std::move(new_pn);
            }
            ++accepted_captures;
            capture_state = Valid;
            processing_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - process_start).count();
        }
        if (dma_lk.owns_lock()) {
            kick_dma();
            dma_lk.unlock();
        }
        if (!samples) {
            lk.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

} // namespace phase_noise
