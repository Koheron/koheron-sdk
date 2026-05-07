/// PhaseNoiseAnalyzer driver
///
/// (c) Koheron

#include "./phase-noise-analyzer.hpp"

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

constexpr std::size_t fft_decimation_steps = 2; // 0 -> 2 FFT segments, 1 -> 3, 2 -> 4, ...

template <typename T, std::size_t N>
auto decimate_by_10(const std::array<T, N>& in) {
    static_assert(N % 10 == 0, "Decimation by 10 requires an input size divisible by 10");
    std::array<T, N / 10> out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        T acc = T{0};
        for (std::size_t j = 0; j < 10; ++j) {
            acc += in[10 * i + j];
        }
        out[i] = acc / 10.0f;
    }
    return out;
}

template <std::size_t Steps, typename T, std::size_t N>
auto build_decimation_chain(const std::array<T, N>& input) {
    static_assert(Steps <= 2, "Increase decimation chain storage for Steps > 2");
    std::array<T, N> d0 = input;
    auto d1 = decimate_by_10(d0);
    auto d2 = decimate_by_10(d1);
    return std::tuple{d0, d1, d2};
}

template <typename Arr>
auto welch_density(sig::Spectrum<float>& sp, Arr& data, sci::units::frequency<float> fs) {
    sp.fs(fs);
    sp.window(sig::windows::hann<float>(data.size()));
    return sp.welch<sig::SpectrumScaling::DENSITY, false>(data);
}

template <typename ArrX, typename ArrY>
auto csd_density(sig::Spectrum<float>& sp, ArrX& x, ArrY& y, sci::units::frequency<float> fs) {
    sp.fs(fs);
    sp.window(sig::windows::hann<float>(x.size()));
    return sp.csd<sig::SpectrumScaling::DENSITY, false>(x, y);
}

template <std::size_t Steps, typename Spectrum0, typename Spectrum1, typename Spectrum2>
auto stitch_segments(const Spectrum0& s0, const Spectrum1& s1, const Spectrum2& s2) {
    constexpr std::size_t n_segments = Steps + 1;
    auto out = s0;
    const std::size_t n_bins = out.size();

    for (std::size_t segment = 0; segment < n_segments; ++segment) {
        const std::size_t start = (segment * n_bins) / n_segments;
        const std::size_t end = ((segment + 1) * n_bins) / n_segments;
        const std::size_t level = Steps - segment;

        if (level == 0) {
            for (std::size_t k = start; k < end; ++k) out[k] = s0[k];
        } else if (level == 1) {
            const std::size_t max_k = std::min(end, s1.size());
            for (std::size_t k = start; k < max_k; ++k) out[k] = s1[k];
        } else {
            const std::size_t max_k = std::min(end, s2.size());
            for (std::size_t k = start; k < max_k; ++k) out[k] = s2[k];
        }
    }

    return out;
}

} // namespace


PhaseNoiseAnalyzer::PhaseNoiseAnalyzer()
: cfg    (services::require<rt::ConfigManager>())
, ltc2157(rt::get_driver<Ltc2157>())
, dds    (rt::get_driver<Dds>())
, ctl    (hw::get_memory<mem::control>())
, sts    (hw::get_memory<mem::status>())
, phase_noise(1 + fft_size / 2)
, averager(1)
// , averager_xy(1)
{
    using namespace sci::units::literals;

    // TODO Use proper voltage range for ADC 1 (only ADC0 now)
    vrange= { 1_V * ltc2157.get_input_voltage_range(0, 0),
              1_V * ltc2157.get_input_voltage_range(0, 1) };

    auto& clk_gen = rt::get_driver<ClockGenerator>();
    clk_gen.set_sampling_frequency(0); // 200 MHz
    fs_adc = Frequency(clk_gen.get_adc_sampling_freq()[0]); // Assume both ADCs have same frequency

    ctl.set_bit<reg::cordic, 0>(); // Phase accumulator on

    load_config();

    // Configure the spectrum analyzer
    spectrum.window(sig::windows::hann<float>(fft_size));
    spectrum.nthreads(2);
    spectrum.fs(fs);
    phase_noise.reserve(1 + fft_size / 2);
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
    cfg.set("PhaseNoiseAnalyzer", "channel", channel);
    cfg.set("PhaseNoiseAnalyzer", "fft_navg", fft_navg);
    cfg.set("PhaseNoiseAnalyzer", "cic_rate", cic_rate);
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[DUTX]", dds.get_dds_freq(0));
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[REFX]", dds.get_dds_freq(1));
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[DUTY]", dds.get_dds_freq(2));
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[REFY]", dds.get_dds_freq(3));
    cfg.save();
}

void PhaseNoiseAnalyzer::set_local_oscillator(uint32_t channel, double freq_hz) {
    if (channel > 3) {
        logf<ERROR>("PhaseNoiseAnalyzer::set_local_oscillator: Invalid DDS channel {}\n", channel);
    }

    dds.set_dds_freq(channel, freq_hz);
    set_frequency_scalings();
    averager.clear();
    averager_xy.clear();
}

// When DUT and REF frequencies are different, the phases at CORDIC outputs
// are scaled by proper frequency ratio.
void PhaseNoiseAnalyzer::set_frequency_scalings() {
    const float ratio_01 = float(dds.get_dds_freq(0)) / float(dds.get_dds_freq(1));

    if (ratio_01 >= 1.0f) {
        ctl.write<reg::scaling0>(1);
        ctl.write<reg::scaling1>(int32_t(ratio_01));
    } else {
        ctl.write<reg::scaling0>(int32_t(ratio_01));
        ctl.write<reg::scaling1>(1);
    }

    const float ratio_23 = float(dds.get_dds_freq(2)) / float(dds.get_dds_freq(3));

    if (ratio_23 >= 1.0f) {
        ctl.write<reg::scaling2>(1);
        ctl.write<reg::scaling3>(int32_t(ratio_23));
    } else {
        ctl.write<reg::scaling2>(int32_t(ratio_23));
        ctl.write<reg::scaling3>(1);
    }
}

void PhaseNoiseAnalyzer::set_cic_rate(uint32_t rate) {
    if (rate < prm::cic_decimation_rate_min ||
        rate > prm::cic_decimation_rate_max) {
        log<ERROR>("PhaseNoiseAnalyzer: CIC rate out of range\n");
        return;
    }

    cic_rate = rate;
    fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
    min_frequency = fs_adc / static_cast<float>(fft_size * cic_rate);
    logf("Minimum frequency = {} Hz\n", min_frequency.eval());
    dma_transfer_duration = prm::n_pts / fs;
    logf("DMA transfer duration = {} s\n", dma_transfer_duration.eval());

    dma.set_fs(fs);
    spectrum.fs(fs);
    averager.clear();
    averager_xy.clear();
    ctl.write<reg::cic_rate>(cic_rate);
}

void PhaseNoiseAnalyzer::set_min_frequency(float min_frequency_hz) {
    min_frequency = Frequency(min_frequency_hz);
    
    if (min_frequency <= sci::units::hertz<float>(0.0f)) {
        log<ERROR>("PhaseNoiseAnalyzer: Minimum frequency must be > 0 Hz\n");
        return;
    }

    auto rate_f = sci::around(fs_adc / (fft_size * min_frequency));
    static_assert(sci::units::is_dimensionless<decltype(rate_f)>);
    auto rate = static_cast<uint32_t>(rate_f.eval());
    rate = std::max(prm::cic_decimation_rate_min, std::min(prm::cic_decimation_rate_max, rate));
    set_cic_rate(rate);
}

void PhaseNoiseAnalyzer::set_channel(uint32_t chan) {
    if (chan != InputChannel::X && chan != InputChannel::Y && chan != InputChannel::XY) {
        log<ERROR>("PhaseNoiseAnalyzer: Invalid channel\n");
        return;
    }

    channel = chan;
    averager.clear();
    averager_xy.clear();
    set_power_conversion_factor();
}

// Carrier power in dBm
double PhaseNoiseAnalyzer::get_carrier_power(uint32_t navg) {
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

PhaseNoiseAnalyzer::PhaseDataArray PhaseNoiseAnalyzer::get_phase_x() {
    using namespace sci::operators;
    phase_x = dma.data_x<data_size>() * calib_factor;
    return phase_x;
}

PhaseNoiseAnalyzer::PhaseDataArray PhaseNoiseAnalyzer::get_phase_y() {
    using namespace sci::operators;
    phase_y = dma.data_y<data_size>() * calib_factor;
    return phase_y;
}

std::array<PhaseNoiseAnalyzer::Phase, 2 * PhaseNoiseAnalyzer::data_size>
PhaseNoiseAnalyzer::get_phase_xy_sync() {
    using namespace sci::operators;
    get_phase_xy();
    return phase_x | phase_y; // Concatenates
}

void PhaseNoiseAnalyzer::get_phase_xy() {
    using namespace sci::operators;
    auto [data_x, data_y] = dma.data_xy<data_size>();
    phase_x = data_x * calib_factor;
    phase_y = data_y * calib_factor;
}

PhaseNoiseAnalyzer::PhaseNoiseDensityVector PhaseNoiseAnalyzer::get_phase_noise() const {
    std::shared_lock lk(data_mtx);
    return phase_noise;
}

void PhaseNoiseAnalyzer::set_fft_navg(uint32_t n_avg) {
    if (n_avg > 200) {
        n_avg = 200;
    }

    fft_navg = n_avg;
    averager.set_navg(fft_navg);
    // averager_xy.set_navg(fft_navg);
}

void PhaseNoiseAnalyzer::reset_cumulative_averager() {
    averager_xy.clear();
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

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[DUTX]")) {
        set_local_oscillator(DdsChannel::DUTX, cfg.get<uint32_t>("PhaseNoiseAnalyzer", "dds_freq[DUTX]"));
    } else {
        set_local_oscillator(DdsChannel::DUTX, 10E6);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[REFX]")) {
        set_local_oscillator(DdsChannel::REFX, cfg.get<uint32_t>("PhaseNoiseAnalyzer", "dds_freq[REFX]"));
    } else {
        set_local_oscillator(DdsChannel::REFX, 10E6);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[DUTY]")) {
        set_local_oscillator(DdsChannel::DUTY, cfg.get<uint32_t>("PhaseNoiseAnalyzer", "dds_freq[DUTY]"));
    } else {
        set_local_oscillator(DdsChannel::DUTY, 10E6);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[REFY]")) {
        set_local_oscillator(DdsChannel::REFY, cfg.get<uint32_t>("PhaseNoiseAnalyzer", "dds_freq[REFY]"));
    } else {
        set_local_oscillator(DdsChannel::REFY, 10E6);
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

    // XY mode has no single ADC channel mapping for carrier-power conversion.
    // Use channel X calibration to avoid out-of-bounds indexing and keep behavior stable.
    const uint32_t adc_channel = (channel == InputChannel::Y) ? 1U : 0U;
    const double Hinv = sci::polynomial::polyval(dds.get_dds_freq(adc_channel),
                                                 ltc2157.tf_polynomial<double>(0, adc_channel));
    const auto power_conv_factor = Hinv * magic_factor * vrange[adc_channel] * vrange[adc_channel] / load;
    conv_factor_dBm = power_conv_factor / 1_mW;

    // Dimensional analysis checks
    static_assert(sci::units::is_power<decltype(power_conv_factor)>);
    static_assert(sci::units::is_dimensionless<decltype(conv_factor_dBm)>);
}

auto PhaseNoiseAnalyzer::compute_phase_noise(PhaseDataArray& new_phase) {
    constexpr std::size_t base_size = 32000;
    static_assert(base_size <= data_size, "base_size must fit acquisition buffer");
    std::array<Phase, base_size> d0{};
    for (std::size_t i = 0; i < base_size; ++i) {
        d0[i] = new_phase[i];
    }

    auto [x0, x1, x2] = build_decimation_chain<fft_decimation_steps>(d0);
    auto s0 = welch_density(spectrum, x0, fs);
    auto s1 = welch_density(spectrum, x1, fs / 10.0f);
    auto s2 = welch_density(spectrum, x2, fs / 100.0f);
    auto phase_psd = stitch_segments<fft_decimation_steps>(s0, s1, s2);

    if (fft_navg > 1) {
        averager.append(std::move(phase_psd));
        return averager.average();
    } else {
        return phase_psd;
    }
}

auto PhaseNoiseAnalyzer::compute_crossed_phase_noise(PhaseDataArray& new_phase_x, PhaseDataArray& new_phase_y) {
    constexpr std::size_t base_size = 32000;
    static_assert(base_size <= data_size, "base_size must fit acquisition buffer");
    std::array<Phase, base_size> x0{};
    std::array<Phase, base_size> y0{};
    for (std::size_t i = 0; i < base_size; ++i) {
        x0[i] = new_phase_x[i];
        y0[i] = new_phase_y[i];
    }

    auto [dx0, dx1, dx2] = build_decimation_chain<fft_decimation_steps>(x0);
    auto [dy0, dy1, dy2] = build_decimation_chain<fft_decimation_steps>(y0);

    auto s0 = csd_density(spectrum, dx0, dy0, fs);
    auto s1 = csd_density(spectrum, dx1, dy1, fs / 10.0f);
    auto s2 = csd_density(spectrum, dx2, dy2, fs / 100.0f);

    auto phase_psd = stitch_segments<fft_decimation_steps>(s0, s1, s2);
    averager_xy.append(phase_psd);
    return sci::real(averager_xy.average());
    // return sci::sqrt(sci::norm(averager_xy.average()));
}

void PhaseNoiseAnalyzer::start_spectrum_analyzer() {
    bool expected = false;
    if (spectrum_analyzer_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        sa_thread = std::thread{&PhaseNoiseAnalyzer::spectrum_analyzer_thread, this};
    }
}

void PhaseNoiseAnalyzer::spectrum_analyzer_thread() {
    while (spectrum_analyzer_started.load(std::memory_order_acquire)) {
        std::shared_lock lk(data_mtx);

        double f_dds;

        if (channel == InputChannel::X) {
            get_phase_x();
            phase_noise = compute_phase_noise(phase_x);
            f_dds = dds.get_dds_freq(DdsChannel::DUTX);
        } else if (channel == InputChannel::Y) {
            get_phase_y();
            phase_noise = compute_phase_noise(phase_y);
            f_dds = dds.get_dds_freq(DdsChannel::DUTY);
        } else if (channel == InputChannel::XY) {
            get_phase_xy();
            phase_noise = compute_crossed_phase_noise(phase_x, phase_y);
            f_dds = dds.get_dds_freq(DdsChannel::DUTY);
        } else {
            logf<ERROR>("PhaseNoiseAnalyzer::spectrum_analyzer_thread: Invalid channel {}\n", channel);
            continue;
        }

        constexpr auto max_phase = 10.0f * sci::pi<Phase>;

        if (sci::absolute(sci::stats::mean(phase_x)) > max_phase ||
            sci::absolute(sci::stats::mean(phase_y)) > max_phase) {
            reset_phase_unwrapper();
        }

        compute_jitter(Frequency(f_dds));
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
            phase_jitter = sci::sqrt(sci::trapz(phase_noise.begin() + 1, phase_noise.end(), df));
        } else {
            f_lo_used = pow10f(low_dec);
            f_hi_used = pow10f(high_dec);

            std::size_t k1 = std::max(1u, static_cast<std::size_t>(sci::ceil(f_lo_used / df).eval()));
            std::size_t k2 = std::min(n_bins - 1u, static_cast<std::size_t>(sci::floor(f_hi_used / df).eval()));

            if (k2 <= k1) {
                k1 = std::max(1u, k1);
                k2 = std::min(n_bins - 1u, std::max(k1 + 1, k2));
            }

            phase_jitter = sci::sqrt(sci::trapz(phase_noise.begin() + k1, phase_noise.begin() + k2 + 1, df));
        }

        time_jitter = phase_jitter / (2.0f * sci::pi<Phase> * f_dut);
    }
}
