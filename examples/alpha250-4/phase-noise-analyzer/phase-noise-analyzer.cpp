/// PhaseNoiseAnalyzer driver
///
/// (c) Koheron

#include "./phase-noise-analyzer.hpp"
#include "./phase-spectrum.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/config_manager.hpp"
#include "boards/alpha250-4/drivers/ltc2157.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <span>
#include <thread>
#include <scicpp/polynomials.hpp>

namespace sci = scicpp;
namespace sig = scicpp::signal;

namespace {
using namespace pna_spectrum;
constexpr float tracking_sign_x = +1.0f;
constexpr float tracking_sign_y = +1.0f;
} // namespace

PhaseNoiseAnalyzer::PhaseNoiseAnalyzer()
: cfg    (services::require<rt::ConfigManager>())
, ltc2157(rt::get_driver<Ltc2157>())
, dds    (rt::get_driver<Dds>())
, ctl    (hw::get_memory<mem::control>())
, sts    (hw::get_memory<mem::status>())
, phase_noise(spectrum_bins)
, averager(1)
{
    using namespace sci::units::literals;

    vrange= { 1_V * ltc2157.get_input_voltage_range(0, 0),
              1_V * ltc2157.get_input_voltage_range(1, 0) };

    auto& clk_gen = rt::get_driver<ClockGenerator>();
    clk_gen.set_sampling_frequency(0); // 200 MHz
    fs_adc = Frequency(clk_gen.get_adc_sampling_freq()[0]); // Assume both ADCs have same frequency

    ctl.set_bit<reg::cordic, 0>(); // Phase accumulator on

    load_config();

    // Configure the spectrum analyzer
    spectrum.window(sig::windows::hann<float>(fft_size));
    spectrum.nthreads(2);
    spectrum.fs(fs);
    phase_noise.reserve(spectrum_bins);
    reset_phase_unwrapper();
    dma.set_fs(fs);
    dma.start_acquisition();
    start_spectrum_analyzer();
}

PhaseNoiseAnalyzer::~PhaseNoiseAnalyzer() {
    spectrum_analyzer_started.store(false, std::memory_order_release);
    if (sa_thread.joinable()) {
        sa_thread.join();
    }
}

void PhaseNoiseAnalyzer::save_config() {
    std::shared_lock lk(data_mtx);
    cfg.set("PhaseNoiseAnalyzer", "channel", channel);
    cfg.set("PhaseNoiseAnalyzer", "fft_navg", fft_navg);
    cfg.set("PhaseNoiseAnalyzer", "cic_rate", cic_rate);
    cfg.set("PhaseNoiseAnalyzer", "phase_precision", phase_precision);
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[DUTX]", base_dds_freq[DdsChannel::DUTX].eval());
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[REFX]", base_dds_freq[DdsChannel::REFX].eval());
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[DUTY]", base_dds_freq[DdsChannel::DUTY].eval());
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[REFY]", base_dds_freq[DdsChannel::REFY].eval());
    cfg.set("PhaseNoiseAnalyzer", "tracking_enabled", tracking_enabled);
    cfg.set("PhaseNoiseAnalyzer", "tracking_bandwidth", tracking_bandwidth.eval());
    cfg.set("PhaseNoiseAnalyzer", "tracking_max_correction", tracking_max_correction.eval());
    cfg.set("PhaseNoiseAnalyzer", "tracking_max_step", tracking_max_step.eval());
    cfg.save();
}

void PhaseNoiseAnalyzer::set_local_oscillator(uint32_t channel, double freq_hz) {
    std::unique_lock lk(data_mtx);
    if (channel > 3) {
        logf<ERROR>("PhaseNoiseAnalyzer::set_local_oscillator: Invalid DDS channel {}\n", channel);
        return;
    }

    if (!std::isfinite(freq_hz) || freq_hz < 0.0 || freq_hz > fs_adc.eval() / 2.0) {
        log<ERROR>("PhaseNoiseAnalyzer::set_local_oscillator: Invalid frequency\n");
        return;
    }
    base_dds_freq[channel] = Frequency(freq_hz);
    tracking_correction[channel] = Frequency{0.0f};
    dds.set_dds_freq(channel, base_dds_freq[channel].eval(), true);
    set_frequency_scalings();
    set_power_conversion_factor();
    restart_filters();
    invalidate_acquisition();
}

void PhaseNoiseAnalyzer::set_tracking_enabled(bool enabled) {
    std::unique_lock lk(data_mtx);
    if (tracking_enabled != enabled) {
        tracking_locks = {};
        tracking_locked = false;
    }
    tracking_enabled = enabled;
}

void PhaseNoiseAnalyzer::set_tracking_bandwidth(float bandwidth_hz) {
    std::unique_lock lk(data_mtx);
    tracking_bandwidth = Frequency(std::max(0.0f, bandwidth_hz));
}

void PhaseNoiseAnalyzer::set_tracking_max_correction(float max_correction_hz) {
    std::unique_lock lk(data_mtx);
    tracking_max_correction = Frequency(std::max(0.0f, max_correction_hz));
}

void PhaseNoiseAnalyzer::set_tracking_max_step(float max_step_hz) {
    std::unique_lock lk(data_mtx);
    tracking_max_step = Frequency(std::max(0.0f, max_step_hz));
}

// When DUT and REF frequencies are different, the phases at CORDIC outputs
// are scaled by proper frequency ratio.
void PhaseNoiseAnalyzer::set_frequency_scalings() {
    const auto x = phase_scaling(base_dds_freq[DUTX].eval(), base_dds_freq[REFX].eval());
    const auto y = phase_scaling(base_dds_freq[DUTY].eval(), base_dds_freq[REFY].eval());
    ctl.write<reg::scaling0>(x.dut);
    ctl.write<reg::scaling1>(x.reference);
    ctl.write<reg::scaling2>(y.dut);
    ctl.write<reg::scaling3>(y.reference);
    phase_scale_x = x.output_scale;
    phase_scale_y = y.output_scale;
}

void PhaseNoiseAnalyzer::set_cic_rate(uint32_t rate) {
    std::unique_lock lk(data_mtx);
    configure_cic_rate(rate);
}

void PhaseNoiseAnalyzer::configure_cic_rate(uint32_t rate) {
    if (rate < prm::cic_decimation_rate_min ||
        rate > prm::cic_decimation_rate_max) {
        log<ERROR>("PhaseNoiseAnalyzer: CIC rate out of range\n");
        return;
    }

    cic_rate = rate;
    cic_output_scale = cic_gain_compensation(rate, prm::cic_n_stages, prm::cic_differential_delay);
    fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
    min_frequency = 2.0 * fs / double(spectrum_samples);
    logf("Sampling frequency = {} Hz\n", fs.eval());
    logf("Minimum frequency = {} Hz (cic_rate = {})\n", min_frequency.eval(), cic_rate);
    dma_transfer_duration = data_size / fs;
    logf("DMA transfer duration = {} s\n", dma_transfer_duration.eval());

    dma.configure_sampling(fs, [this] { ctl.write<reg::cic_rate>(cic_rate); });
    spectrum.fs(fs);
    invalidate_acquisition();
}

void PhaseNoiseAnalyzer::restart_filters() {
    // The FPGA restarts both filters/FIFOs and unwrappers on this epoch toggle.
    // Wait until a complete X/Y DMA pair has finished before changing hardware.
    dma.configure_sampling(fs, [this] {
        hardware_epoch ^= 0x100u;
        ctl.write<reg::phase_precision>(phase_precision | hardware_epoch);
    });
}

bool PhaseNoiseAnalyzer::set_phase_precision(uint32_t bits) {
    if (bits > 8) return false;
    std::unique_lock lk(data_mtx);
    phase_precision = bits;
    restart_filters();
    invalidate_acquisition();
    return true;
}

void PhaseNoiseAnalyzer::set_min_frequency(float min_frequency_hz) {
    if (!std::isfinite(min_frequency_hz) || min_frequency_hz <= 0.0f) {
        log<ERROR>("PhaseNoiseAnalyzer: Minimum frequency must be finite and > 0 Hz\n");
        return;
    }
    std::unique_lock lk(data_mtx);
    const double rate_f = std::clamp(
        std::round(fs_adc.eval() / (spectrum_samples * double(min_frequency_hz))),
        double(prm::cic_decimation_rate_min), double(prm::cic_decimation_rate_max));
    configure_cic_rate(static_cast<uint32_t>(rate_f));
}

void PhaseNoiseAnalyzer::set_channel(uint32_t chan) {
    std::unique_lock lk(data_mtx);
    if (chan != InputChannel::X && chan != InputChannel::Y && chan != InputChannel::XY) {
        log<ERROR>("PhaseNoiseAnalyzer: Invalid channel\n");
        return;
    }

    channel = chan;
    invalidate_acquisition();
    set_power_conversion_factor();
}

// Carrier power in dBm
double PhaseNoiseAnalyzer::get_carrier_power(uint32_t navg) {
    std::shared_lock lk(data_mtx);
    return carrier_power(navg);
}

double PhaseNoiseAnalyzer::carrier_power(uint32_t navg) {
    if (navg == 0) return std::numeric_limits<double>::quiet_NaN();
    uint32_t demod_raw;
    double res = 0.0;

    for (uint32_t i=0; i<navg; ++i) {
        if (channel != InputChannel::Y) {
            demod_raw = sts.read<reg::demod0, uint32_t>();
        } else {
            demod_raw = sts.read<reg::demod2, uint32_t>();
        }

        // Extract real and imaginary parts and convert fix16_0 to float to obtain complex IQ signal
        const auto z = std::complex(static_cast<int16_t>(demod_raw & 0xFFFF) / 65536.0,
                                    static_cast<int16_t>((demod_raw >> 16) & 0xFFFF) / 65536.0);
        res += std::norm(z);
    }

    return 10.0 * sci::log10(conv_factor_dBm * res / double(navg));
}

PhaseNoiseAnalyzer::PhaseDataArray PhaseNoiseAnalyzer::get_phase_x() {
    std::shared_lock lk(data_mtx);
    return phase_x;
}

PhaseNoiseAnalyzer::PhaseDataArray PhaseNoiseAnalyzer::get_phase_y() {
    std::shared_lock lk(data_mtx);
    return phase_y;
}

std::array<PhaseNoiseAnalyzer::Phase, 2 * PhaseNoiseAnalyzer::data_size>
PhaseNoiseAnalyzer::get_phase_xy_sync() {
    using namespace sci::operators;
    std::shared_lock lk(data_mtx);
    return phase_x | phase_y;
}

PhaseNoiseAnalyzer::PhaseNoiseDensityVector PhaseNoiseAnalyzer::get_phase_noise() const {
    std::shared_lock lk(publication_mtx);
    return published_phase_noise;
}

void PhaseNoiseAnalyzer::publish_spectrum() {
    std::unique_lock lk(publication_mtx);
    published_phase_noise = phase_noise;
    published_count = channel == XY ? averager_xy.count() : averager.count();
    published_target = channel == XY ? 0u : fft_navg;
}

void PhaseNoiseAnalyzer::set_fft_navg(uint32_t n_avg) {
    std::unique_lock lk(data_mtx);
    fft_navg = std::clamp(n_avg, 1u, 200u);
    averager.set_navg(fft_navg);
    publish_spectrum();
}

void PhaseNoiseAnalyzer::reset_cumulative_averager() {
    reset_cumulative_requested.store(true, std::memory_order_release);
}

// ----------------- Private functions

void PhaseNoiseAnalyzer::load_config() {
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

    const auto bits = cfg.has("PhaseNoiseAnalyzer", "phase_precision")
        ? cfg.get<uint32_t>("PhaseNoiseAnalyzer", "phase_precision") : 0u;
    if (!set_phase_precision(bits)) set_phase_precision(0);

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[DUTX]")) {
        set_local_oscillator(DdsChannel::DUTX, cfg.get<double>("PhaseNoiseAnalyzer", "dds_freq[DUTX]"));
    } else {
        set_local_oscillator(DdsChannel::DUTX, 10E6);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[REFX]")) {
        set_local_oscillator(DdsChannel::REFX, cfg.get<double>("PhaseNoiseAnalyzer", "dds_freq[REFX]"));
    } else {
        set_local_oscillator(DdsChannel::REFX, 10E6);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[DUTY]")) {
        set_local_oscillator(DdsChannel::DUTY, cfg.get<double>("PhaseNoiseAnalyzer", "dds_freq[DUTY]"));
    } else {
        set_local_oscillator(DdsChannel::DUTY, 10E6);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[REFY]")) {
        set_local_oscillator(DdsChannel::REFY, cfg.get<double>("PhaseNoiseAnalyzer", "dds_freq[REFY]"));
    } else {
        set_local_oscillator(DdsChannel::REFY, 10E6);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "tracking_enabled")) {
        set_tracking_enabled(cfg.get<bool>("PhaseNoiseAnalyzer", "tracking_enabled"));
    }
    if (cfg.has("PhaseNoiseAnalyzer", "tracking_bandwidth")) {
        set_tracking_bandwidth(cfg.get<float>("PhaseNoiseAnalyzer", "tracking_bandwidth"));
    }
    if (cfg.has("PhaseNoiseAnalyzer", "tracking_max_correction")) {
        set_tracking_max_correction(cfg.get<float>("PhaseNoiseAnalyzer", "tracking_max_correction"));
    }
    if (cfg.has("PhaseNoiseAnalyzer", "tracking_max_step")) {
        set_tracking_max_step(cfg.get<float>("PhaseNoiseAnalyzer", "tracking_max_step"));
    }
}

void PhaseNoiseAnalyzer::reset_phase_unwrapper() {
    ctl.set_bit<reg::cordic, 1>();
    ctl.clear_bit<reg::cordic, 1>();
}

void PhaseNoiseAnalyzer::set_power_conversion_factor() {
    using namespace sci::units::literals;
    constexpr auto load = 50_Ohm;
    constexpr double magic_factor = 22.0;

    // Report the DUT on X in XY mode, and the DUT on Y in Y mode.
    const uint32_t adc = (channel == InputChannel::Y) ? 1U : 0U;
    const uint32_t dds_channel = adc == 0 ? DUTX : DUTY;
    const double Hinv = sci::polynomial::polyval(dds.get_dds_freq(dds_channel),
                                                 ltc2157.tf_polynomial<double>(adc, 0));
    const auto power_conv_factor = Hinv * magic_factor * vrange[adc] * vrange[adc] / load;
    conv_factor_dBm = power_conv_factor / 1_mW;

    // Dimensional analysis checks
    static_assert(sci::units::is_power<decltype(power_conv_factor)>);
    static_assert(sci::units::is_dimensionless<decltype(conv_factor_dBm)>);
}

auto PhaseNoiseAnalyzer::compute_phase_noise(SpectrumPhaseArray& new_phase) {
    constexpr std::size_t base_size = 32000;
    static_assert(base_size <= data_size, "base_size must fit acquisition buffer");

    auto d0 = new_phase; // Already detrended in raw integer counts.

    auto [x0, x1, x2] = build_decimation_chain<fft_decimation_steps>(d0);

    auto s0 = welch_density(spectrum, x0, fs);
    auto s1 = welch_density(spectrum, x1, fs / 10.0f);
    auto s2 = welch_density(spectrum, x2, fs / 100.0f);

    compensate_decimated_psd<1>(s1);
    compensate_decimated_psd<2>(s2);

    auto phase_psd = stitch_segments(s0, s1, s2);

    // Keep the one-window history current when averaging is disabled so growing
    // the window cannot bring back spectra from before the unaveraged interval.
    averager.append(std::move(phase_psd));
    return averager.average();
}

auto PhaseNoiseAnalyzer::compute_crossed_phase_noise(SpectrumPhaseArray& new_phase_x,
                                                     SpectrumPhaseArray& new_phase_y) {
    auto phase_psd = pna_spectrum::cross_density(new_phase_x, new_phase_y,
        sci::units::frequency<float>{float(fs.eval())}, spectrum, false);

    averager_xy.append(phase_psd);
    return sci::real(averager_xy.average());
}

PhaseNoiseAnalyzer::Frequency PhaseNoiseAnalyzer::effective_tracking_bandwidth() const {
    return sci::units::fmin(tracking_bandwidth,
                            sci::units::fmin(min_frequency / 100.0f, Frequency{0.1f}));
}

void PhaseNoiseAnalyzer::apply_tracking_update(Phase mean_dphi, Time block_duration, uint32_t input_channel) {
    using namespace sci::operators;

    auto clamp_freq = [](Frequency v, Frequency lo, Frequency hi) {
        return sci::units::fmin(hi, sci::units::fmax(lo, v));
    };

    if (!tracking_enabled ||
        (input_channel != Y && (base_dds_freq[DUTX] <= Frequency{0.0} ||
                                base_dds_freq[REFX] <= Frequency{0.0})) ||
        (input_channel != X && (base_dds_freq[DUTY] <= Frequency{0.0} ||
                                base_dds_freq[REFY] <= Frequency{0.0}))) {
        return;
    }

    // Keep the tracking loop at very low bandwidth so it does not perturb phase-noise offsets.
    const auto bw = effective_tracking_bandwidth();
    if (bw <= Frequency{0.0f} || block_duration <= Time{0.0f}) {
        return;
    }

    const auto alpha_raw = 1.0 - sci::exp(-2.0 * sci::pi<double> * bw * block_duration);
    if (alpha_raw <= 0.0) {
        return;
    }
    const auto alpha = std::min(alpha_raw, 0.2);

    // Positive mean_dphi means input phase advances relative to DDS; increase DDS frequency.
    const auto raw_f_error = static_cast<double>(mean_dphi.eval()) * fs / (2.0 * sci::pi<double>);
    const auto f_error = clamp_freq(raw_f_error,
                                    -tracking_max_step / alpha,
                                    tracking_max_step / alpha);

    // Phase samples have already been restored to DUT radians after FPGA scaling.
    const auto step = clamp_freq(alpha * f_error, -tracking_max_step, tracking_max_step);

    tracking_last_mean_dphi = mean_dphi;
    tracking_last_error = f_error;

    auto apply_to_dut = [&](uint32_t dds_channel) {
        tracking_correction[dds_channel] = clamp_freq(
            tracking_correction[dds_channel] + step,
            -tracking_max_correction,
            tracking_max_correction
        );
        const auto corrected = base_dds_freq[dds_channel] - tracking_correction[dds_channel];
        dds.set_dds_freq(dds_channel, corrected.eval(), false);
    };

    if (input_channel == InputChannel::X) {
        apply_to_dut(DdsChannel::DUTX);
    } else if (input_channel == InputChannel::Y) {
        apply_to_dut(DdsChannel::DUTY);
    } else {
        apply_to_dut(DdsChannel::DUTX);
        apply_to_dut(DdsChannel::DUTY);
    }

    tracking_locked = tracking_locks[input_channel == Y ? 1 : 0].update(f_error.eval(), alpha, 0.1 * tracking_max_step.eval());
}

void PhaseNoiseAnalyzer::start_spectrum_analyzer() {
    bool expected = false;
    if (spectrum_analyzer_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        sa_thread = std::thread{&PhaseNoiseAnalyzer::spectrum_analyzer_thread, this};
    }
}

void PhaseNoiseAnalyzer::invalidate_acquisition() {
    capture_state = Settling;
    phase_x.fill(Phase{});
    phase_y.fill(Phase{});
    tracking_locks = {};
    tracking_locked = false;
    averager.clear();
    averager_xy.clear();
    phase_noise.assign(spectrum_bins, PhaseNoiseDensity{});
    publish_spectrum();
    phase_jitter = std::numeric_limits<Phase>::quiet_NaN();
    time_jitter = std::numeric_limits<Time>::quiet_NaN();
    f_lo_used = std::numeric_limits<Frequency>::quiet_NaN();
    f_hi_used = std::numeric_limits<Frequency>::quiet_NaN();
    ++acquisition_epoch;
}

void PhaseNoiseAnalyzer::spectrum_analyzer_thread() {
    uint64_t consumed = 0;
    uint64_t epoch;
    {
        std::shared_lock lk(data_mtx);
        epoch = acquisition_epoch;
    }
    while (spectrum_analyzer_started.load(std::memory_order_acquire)) {
        {
            std::unique_lock lk(data_mtx);
            if (epoch != acquisition_epoch) {
                epoch = acquisition_epoch;
                // Drain queued samples generated under the previous configuration.
                consumed = dma.completed_chunks() + discard_acquisitions_after_reset;
            }
            if (reset_cumulative_requested.exchange(false, std::memory_order_acq_rel)) {
                averager_xy.clear();
                if (channel == XY) {
                    phase_noise.assign(spectrum_bins, PhaseNoiseDensity{});
                    publish_spectrum();
                }
                consumed = std::max(consumed, dma.completed_chunks());
                tracking_last_mean_dphi = Phase{0.0f};
                tracking_last_error = Frequency{0.0f};
                tracking_locked = false;
                tracking_locks = {};
            }
        }

        auto snapshot = dma.read_xy<data_size>(consumed, spectrum_analyzer_started);
        if (!snapshot) {
            std::unique_lock lk(data_mtx);
            invalidate_acquisition();
            ++dma_errors;
            capture_state = DmaError;
            return;
        }

        std::unique_lock lk(data_mtx);
        if (epoch != acquisition_epoch || reset_cumulative_requested.load(std::memory_order_acquire)) {
            continue;
        }
        const Time block_duration = double(snapshot->end_chunk - consumed) * PhaseDma::samples_per_chunk / fs;
        consumed = snapshot->end_chunk;
        const auto now = std::chrono::steady_clock::now();
        if (last_capture_time != std::chrono::steady_clock::time_point{})
            capture_period_ms = std::chrono::duration<double, std::milli>(now - last_capture_time).count();
        last_capture_time = now;
        captured_precision = snapshot->precision_x;
        if (snapshot->overflow) {
            ++overflow_captures;
            invalidate_acquisition();
            // Continuous unwrappers cannot retain an overflow forever. Rebase
            // together and drain their filter history; limit retries to 1 Hz.
            if (now - last_overrange_reset >= std::chrono::seconds(1)) {
                restart_filters();
                last_overrange_reset = now;
            }
            capture_state = Overrange;
            continue;
        }
        if (!snapshot->matches_precision(phase_precision)) {
            invalidate_acquisition();
            continue;
        }
        const auto process_start = std::chrono::steady_clock::now();
        const auto scale_x = calib_factor * float(cic_output_scale * phase_scale_x * std::exp2(-double(phase_precision)));
        const auto scale_y = calib_factor * float(cic_output_scale * phase_scale_y * std::exp2(-double(phase_precision)));
        convert_relative_phase(snapshot->x, phase_x, scale_x);
        convert_relative_phase(snapshot->y, phase_y, scale_y);
        const auto trend_x = phase_noise::fit_raw_phase_prefix<32000>(snapshot->x);
        const auto trend_y = phase_noise::fit_raw_phase_prefix<32000>(snapshot->y);
        auto spectral_x = phase_noise::detrended_raw_phase_prefix<32000>(snapshot->x, trend_x, scale_x);
        auto spectral_y = phase_noise::detrended_raw_phase_prefix<32000>(snapshot->y, trend_y, scale_y);
        const auto slope_x = Phase{float(trend_x.slope * double(scale_x.eval()))};
        const auto slope_y = Phase{float(trend_y.slope * double(scale_y.eval()))};

        const double f_dds = dds.get_dds_freq(channel == Y ? DUTY : DUTX);
        if (channel == X) {
            phase_noise = compute_phase_noise(spectral_x);
            apply_tracking_update(tracking_sign_x * slope_x, block_duration, X);
        } else if (channel == Y) {
            phase_noise = compute_phase_noise(spectral_y);
            apply_tracking_update(tracking_sign_y * slope_y, block_duration, Y);
        } else {
            phase_noise = compute_crossed_phase_noise(spectral_x, spectral_y);
            apply_tracking_update(tracking_sign_x * slope_x, block_duration, X);
            const auto x_dphi = tracking_last_mean_dphi;
            const auto x_error = tracking_last_error;
            const bool x_locked = tracking_locked;
            apply_tracking_update(tracking_sign_y * slope_y, block_duration, Y);
            tracking_last_mean_dphi = 0.5f * (x_dphi + tracking_last_mean_dphi);
            tracking_last_error = 0.5 * (x_error + tracking_last_error);
            tracking_locked = x_locked && tracking_locked;
        }
        ++accepted_captures;
        capture_state = Valid;
        processing_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - process_start).count();
        compute_jitter(Frequency(f_dds));
        publish_spectrum();
    }
}

void PhaseNoiseAnalyzer::compute_jitter(Frequency f_dut) {
    if (sci::almost_equal(f_dut, Frequency{0.0f})) {
        // No demodulation if DSS frequency is zero
        phase_jitter = std::numeric_limits<Phase>::quiet_NaN();
        time_jitter  = std::numeric_limits<Time>::quiet_NaN();
        f_lo_used    = std::numeric_limits<Frequency>::quiet_NaN();
        f_hi_used    = std::numeric_limits<Frequency>::quiet_NaN();
    } else {
        const std::size_t n_bins = phase_noise.size();
        const auto df = fs / float(2 * (phase_noise.size() - 1));
        const auto f_min_avail = 2.0 * df;
        const auto f_max_avail = 0.75 * (n_bins - 1) * df;

        auto log10f = [](Frequency f) {
            const auto fmin = std::numeric_limits<Frequency>::min();
            return std::log10(sci::units::fmax(f, fmin).eval());
        };

        auto pow10f = [](float x) {
            return Frequency(std::pow(10.0f, x));
        };

        const float low_dec  = std::ceil(log10f(f_min_avail));
        const float high_dec = std::floor(log10f(f_max_avail));

        const auto tracking_lo = tracking_enabled ? 10.0f * effective_tracking_bandwidth() : Frequency{0.0f};

        if (high_dec <= low_dec) {
            // No full decade: integrate whole available band (excluding DC)
            f_lo_used = sci::units::fmax(f_min_avail, tracking_lo);
            f_hi_used = f_max_avail;
        } else {
            f_lo_used = pow10f(low_dec);
            f_lo_used = sci::units::fmax(f_lo_used, tracking_lo);
            f_hi_used = pow10f(high_dec);
        }
        if (!(f_lo_used < f_hi_used)) {
            phase_jitter = std::numeric_limits<Phase>::quiet_NaN();
            time_jitter  = std::numeric_limits<Time>::quiet_NaN();
            f_lo_used    = std::numeric_limits<Frequency>::quiet_NaN();
            f_hi_used    = std::numeric_limits<Frequency>::quiet_NaN();
            return;
        }

        const std::size_t k1 = std::max(1u, static_cast<std::size_t>(sci::ceil(f_lo_used / df).eval()));
        const std::size_t k2 = std::min(n_bins - 1u, static_cast<std::size_t>(sci::floor(f_hi_used / df).eval()));
        if (k2 <= k1) {
            phase_jitter = std::numeric_limits<Phase>::quiet_NaN();
            time_jitter  = std::numeric_limits<Time>::quiet_NaN();
            f_lo_used    = std::numeric_limits<Frequency>::quiet_NaN();
            f_hi_used    = std::numeric_limits<Frequency>::quiet_NaN();
            return;
        }
        phase_jitter = sci::sqrt(sci::trapz(phase_noise.begin() + k1, phase_noise.begin() + k2 + 1, sci::units::frequency<float>(df.eval())));

        time_jitter = phase_jitter / (2.0f * sci::pi<Phase> * sci::units::frequency<float>(f_dut.eval()));
    }
}
