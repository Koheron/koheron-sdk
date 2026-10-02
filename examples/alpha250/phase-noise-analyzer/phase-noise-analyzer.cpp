/// PhaseNoiseAnalyzer driver
///
/// (c) Koheron

#include "./phase-noise-analyzer.hpp"
#include "server/drivers/phase-noise/phase-processing.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/config_manager.hpp"
#include "server/drivers/dma-s2mm.hpp"
#include "boards/alpha250/drivers/ltc2157.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <optional>
#include <thread>
#include <scicpp/polynomials.hpp>

namespace sci = scicpp;
namespace sig = scicpp::signal;

PhaseNoiseAnalyzer::PhaseNoiseAnalyzer()
: cfg    (services::require<rt::ConfigManager>())
, dma    (rt::get_driver<DmaS2MM>())
, ltc2157(rt::get_driver<Ltc2157>())
, dds    (rt::get_driver<Dds>())
, ctl    (hw::get_memory<mem::control>())
, sts    (hw::get_memory<mem::status>())
, phase_noise(1 + fft_size / 2)
, averager(1)
{
    using namespace sci::units::literals;
    vrange= { 1_V * ltc2157.get_input_voltage_range(0),
              1_V * ltc2157.get_input_voltage_range(1) };

    auto& clk_gen = rt::get_driver<ClockGenerator>();
    clk_gen.set_sampling_frequency(0); // 200 MHz
    fs_adc = Frequency(clk_gen.get_adc_sampling_freq());

    ctl.write_mask<reg::cordic, 0b11>(0b11); // Phase accumulator on

    load_config();

    // Configure the spectrum analyzer
    spectrum.window(sig::windows::hann<float>(fft_size));
    spectrum.nthreads(2);
    spectrum.fs(fs);
    phase_noise.reserve(1 + fft_size / 2);
    start_acquisition();
}

PhaseNoiseAnalyzer::~PhaseNoiseAnalyzer() {
    acquisition_started.store(false, std::memory_order_release);
    if (acq_thread.joinable()) {
        acq_thread.join();
    }
}

void PhaseNoiseAnalyzer::save_config() {
    std::unique_lock lk(data_mtx);
    cfg.set("PhaseNoiseAnalyzer", "channel", channel);
    cfg.set("PhaseNoiseAnalyzer", "fft_navg", fft_navg);
    cfg.set("PhaseNoiseAnalyzer", "cic_rate", cic_rate);
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[0]", dds.get_dds_freq(0));
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[1]", dds.get_dds_freq(1));
    cfg.set("PhaseNoiseAnalyzer", "analyzer_mode", analyzer_mode);
    cfg.set("PhaseNoiseAnalyzer", "interferometer_delay", interferometer_delay.eval());
    cfg.save();
}

void PhaseNoiseAnalyzer::set_local_oscillator(uint32_t lo_channel, double freq_hz) {
    std::unique_lock lk(data_mtx);
    if (lo_channel >= 2 || !std::isfinite(freq_hz) || freq_hz < 0.0 ||
        freq_hz > static_cast<double>(fs_adc.eval()) / 2.0) {
        log<ERROR>("PhaseNoiseAnalyzer: Invalid local oscillator setting\n");
        return;
    }
    dds.set_dds_freq(lo_channel, freq_hz);
    set_power_conversion_factor();
    dirty_cnt = 4;
    invalidate_results();
}

void PhaseNoiseAnalyzer::set_cic_rate(uint32_t rate) {
    if (rate < prm::cic_decimation_rate_min ||
        rate > prm::cic_decimation_rate_max) {
        log<ERROR>("PhaseNoiseAnalyzer: CIC rate out of range\n");
        return;
    }

    std::unique_lock dma_lk(dma_mtx); // block until any DMA transfer finishes
    std::unique_lock lk(data_mtx);

    cic_rate = rate;
    phase_conversion_factor = float(phase_calibration::filter_correction(
        rate, prm::cic_n_stages, prm::cic_differential_delay)) * sci::pi<Phase> / 8192.0f;
    fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
    dma_transfer_duration = prm::n_pts / fs;
    logf("DMA transfer duration = {} s\n", dma_transfer_duration.eval());

    update_interferometer_transfer_function();
    spectrum.fs(fs);
    invalidate_results();
    dirty_cnt = 2;
    ctl.write<reg::cic_rate>(cic_rate);
}

void PhaseNoiseAnalyzer::set_channel(uint32_t chan) {
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
double PhaseNoiseAnalyzer::get_carrier_power(uint32_t navg) {
    std::shared_lock lk(data_mtx);
    return carrier_power(navg);
}

double PhaseNoiseAnalyzer::carrier_power(uint32_t navg) {
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

PhaseNoiseAnalyzer::PhaseDataArray PhaseNoiseAnalyzer::get_phase() const {
    std::shared_lock lk(data_mtx);
    return phase;
}

PhaseNoiseAnalyzer::PhaseNoiseDensityVector PhaseNoiseAnalyzer::get_phase_noise() const {
    std::shared_lock lk(data_mtx);
    return phase_noise;
}

void PhaseNoiseAnalyzer::set_fft_navg(uint32_t n_avg) {
    std::unique_lock lk(data_mtx);
    fft_navg = std::clamp(n_avg, 1u, 100u);
    averager.set_navg(fft_navg);
}

void PhaseNoiseAnalyzer::set_analyzer_mode(uint32_t mode) {
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

void PhaseNoiseAnalyzer::set_interferometer_delay(float delay_s) {
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
}

void PhaseNoiseAnalyzer::reset_phase_unwrapper() {
    ctl.write_mask<reg::cordic, 0b1100>(0b1100);
    ctl.write_mask<reg::cordic, 0b1100>(0b0000);
}

void PhaseNoiseAnalyzer::kick_dma() {
    reset_phase_unwrapper();
    dma.start_transfer<mem::ram, prm::n_pts, int32_t>();
}

auto PhaseNoiseAnalyzer::read_dma() {
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

void PhaseNoiseAnalyzer::update_interferometer_transfer_function() {
    using namespace sci::operators;
    auto freqs = sig::rfftfreq<fft_size>(1.0f / fs);
    interferometer_tf = std::move(sci::pow<2>(
        0.5f / sci::sin(sci::pi<Phase> * std::move(freqs) * interferometer_delay)));
}

void PhaseNoiseAnalyzer::set_power_conversion_factor() {
    using namespace sci::units::literals;
    constexpr auto load = 50_Ohm;
    constexpr double magic_factor = 22.0;

    const double Hinv = sci::polynomial::polyval(dds.get_dds_freq(channel),
                                                 ltc2157.tf_polynomial<double>(channel));
    const auto power_conv_factor = Hinv * magic_factor * vrange[channel] * vrange[channel] / load;
    conv_factor_dBm = power_conv_factor / 1_mW;

    // Dimensional analysis checks
    static_assert(sci::units::is_power<decltype(power_conv_factor)>);
    static_assert(sci::units::is_dimensionless<decltype(conv_factor_dBm)>);
}

auto PhaseNoiseAnalyzer::compute_phase_noise(PhaseDataArray& new_phase) {
    auto detrended = detrended_phase_prefix<data_size>(new_phase);
    auto phase_psd = spectrum.welch<sig::SpectrumScaling::DENSITY, false>(detrended);

    if (analyzer_mode == AnalyzerMode::LASER) {
        using namespace sci::operators;
        phase_psd = std::move(phase_psd) * interferometer_tf;
    }

    // Keep one-window history current even while averaging is disabled.
    averager.append(std::move(phase_psd));
    return averager.average();
}

auto PhaseNoiseAnalyzer::compute_jitter(const PhaseNoiseDensityVector& new_pn) {
    const auto f_dss = Frequency(dds.get_dds_freq(channel));

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

void PhaseNoiseAnalyzer::start_acquisition() {
    bool expected = false;
    if (acquisition_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        acq_thread = std::thread{&PhaseNoiseAnalyzer::acquisition_thread, this};
    }
}

void PhaseNoiseAnalyzer::invalidate_results() {
    averager.clear();
    phase.fill(Phase{});
    phase_noise.assign(1 + fft_size / 2, PhaseNoiseDensity{});
    phase_jitter = std::numeric_limits<Phase>::quiet_NaN();
    time_jitter = std::numeric_limits<Time>::quiet_NaN();
    f_lo_used = std::numeric_limits<Frequency>::quiet_NaN();
    f_hi_used = std::numeric_limits<Frequency>::quiet_NaN();
}

void PhaseNoiseAnalyzer::acquisition_thread() {
    {
        std::unique_lock dma_lk(dma_mtx);
        std::unique_lock lk(data_mtx);
        kick_dma();
    }

    while (acquisition_started.load(std::memory_order_acquire)) {
        std::unique_lock dma_lk(dma_mtx);
        auto samples = read_dma(); // wait without blocking snapshot getters
        std::unique_lock lk(data_mtx);
        if (!acquisition_started.load(std::memory_order_acquire)) break;

        kick_dma(); // run the next transfer while computing this spectrum
        dma_lk.unlock();

        if (!samples) {
            dirty_cnt = std::max(dirty_cnt, 2u);
            invalidate_results();
            lk.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (dirty_cnt > 0) {
            --dirty_cnt;
            continue;
        }

        PhaseDataArray new_phase{};
        convert_relative_phase(*samples, new_phase, phase_conversion_factor);
        auto new_pn = compute_phase_noise(new_phase);
        compute_jitter(new_pn);
        phase = std::move(new_phase);
        phase_noise = std::move(new_pn);
    }
}
