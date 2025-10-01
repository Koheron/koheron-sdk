/// PhaseNoiseAnalyzer driver
///
/// (c) Koheron

#include "./phase-noise-analyzer.hpp"

// PhaseNoiseAnalyzer::PhaseNoiseAnalyzer(Context& ctx_)
// : ctx(ctx_)
// , dma(ctx.get<DmaS2MM>())
// , clk_gen(ctx.get<ClockGenerator>())
// , ltc2157(ctx.get<Ltc2157>())
// , dds(ctx.get<Dds>())
// , ctl(ctx.mm.get<mem::control>())
// , sts(ctx.mm.get<mem::status>())
// , ram(ctx.mm.get<mem::ram>())
// , phase_noise(1 + fft_size / 2)
// , averager(1)
// , interferometer_tf(1 + fft_size / 2)
// {
//     using namespace sci::units::literals;
//     vrange= { 1_V * ltc2157.get_input_voltage_range(0),
//                 1_V * ltc2157.get_input_voltage_range(1) };

//     clk_gen.set_sampling_frequency(0); // 200 MHz

//     ctl.write_mask<reg::cordic, 0b11>(0b11); // Phase accumulator on
//     set_channel(0);
//     set_fft_navg(1);

//     fs_adc = Frequency(clk_gen.get_adc_sampling_freq());
//     set_cic_rate(prm::cic_decimation_rate_default);

//     // Configure the spectrum analyzer
//     spectrum.window(win::hann<float>(fft_size));
//     spectrum.nthreads(2);
//     spectrum.fs(fs);
//     phase_noise.reserve(1 + fft_size / 2);
//     start_acquisition();
// }

void PhaseNoiseAnalyzer::set_local_oscillator(uint32_t channel, double freq_hz) {
    dirty_cnt = 4;
    dds.set_dds_freq(channel, freq_hz);
    averager.clear();
}

void PhaseNoiseAnalyzer::set_cic_rate(uint32_t rate) {
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

    update_interferometer_transfer_function();
    spectrum.fs(fs);
    averager.clear();
    dirty_cnt = 2;
    ctl.write<reg::cic_rate>(cic_rate);
}

void PhaseNoiseAnalyzer::set_channel(uint32_t chan) {
    if (chan != 0 && chan != 1) {
        ctx.log<ERROR>("PhaseNoiseAnalyzer: Invalid channel\n");
        return;
    }

    channel = chan;
    averager.clear();
    set_power_conversion_factor();
    ctl.write_mask<reg::cordic, 0b10000>((channel & 1) << 4);
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

PhaseNoiseAnalyzer::PhaseDataArray PhaseNoiseAnalyzer::get_phase() const {
    std::shared_lock lk(data_mtx);
    return phase;
}

PhaseNoiseAnalyzer::PhaseNoiseDensityVector PhaseNoiseAnalyzer::get_phase_noise() const {
    std::shared_lock lk(data_mtx);
    return phase_noise;
}

void PhaseNoiseAnalyzer::set_fft_navg(uint32_t n_avg) {
    if (n_avg > 100) {
        n_avg = 100;
    }

    fft_navg = n_avg;
    averager.set_navg(fft_navg);
}

void PhaseNoiseAnalyzer::set_analyzer_mode(uint32_t mode) {
    if (mode != AnalyzerMode::RF && mode != AnalyzerMode::LASER) {
        ctx.logf<WARNING>("PhaseNoiseAnalyzer: Invalid mode {}", mode);
        return;
    }

    analyzer_mode = mode;
}

void PhaseNoiseAnalyzer::set_interferometer_delay(float delay_s) {
    interferometer_delay = Time(delay_s);
    ctx.logf<INFO>("PhaseNoiseAnalyzer: Interferometer delay set to {} ns",
                    interferometer_delay.eval() * 1E9f);

    update_interferometer_transfer_function();
}

// ----------------- Private functions

void PhaseNoiseAnalyzer::reset_phase_unwrapper() {
    ctl.write_mask<reg::cordic, 0b1100>(0b1100);
    ctl.write_mask<reg::cordic, 0b1100>(0b0000);
}

void PhaseNoiseAnalyzer::kick_dma() {
    std::scoped_lock lk(dma_mtx);
    reset_phase_unwrapper();
    dma.start_transfer<mem::ram, prm::n_pts, int32_t>();
}

auto PhaseNoiseAnalyzer::read_dma() {
    std::scoped_lock lk(dma_mtx);
    dma.wait_for_transfer(dma_transfer_duration);
    return ram.read_array<int32_t, data_size, read_offset>();
}

void PhaseNoiseAnalyzer::update_interferometer_transfer_function() {
    using namespace sci::operators;
    auto freqs = sig::rfftfreq(fft_size, 1.0f / fs);
    auto tf_0 = 0.5f / sci::sin(sci::pi<Phase> * std::move(freqs) * interferometer_delay);
    interferometer_tf = std::move(tf_0) * std::move(tf_0);
}

void PhaseNoiseAnalyzer::set_power_conversion_factor() {
    using namespace sci::units::literals;
    constexpr auto load = 50_Ohm;
    constexpr double magic_factor = 22.0;

    const double Hinv = poly::polyval(dds.get_dds_freq(channel),
                                        ltc2157.tf_polynomial<double>(channel));
    const auto power_conv_factor = Hinv * magic_factor * vrange[channel] * vrange[channel] / load;
    conv_factor_dBm = power_conv_factor / 1_mW;

    // Dimensional analysis checks
    static_assert(sci::units::is_power<decltype(power_conv_factor)>);
    static_assert(sci::units::is_dimensionless<decltype(conv_factor_dBm)>);
}

auto PhaseNoiseAnalyzer::compute_phase_noise(PhaseDataArray& new_phase) {
    auto phase_psd = spectrum.welch<sig::DENSITY, false>(new_phase);

    if (analyzer_mode == AnalyzerMode::LASER) {
        using namespace sci::operators;
        phase_psd = std::move(phase_psd) * interferometer_tf;
    }

    if (fft_navg > 1) {
        averager.append(std::move(phase_psd));
        return averager.average();
    } else {
        return phase_psd;
    }
}

auto PhaseNoiseAnalyzer::compute_jitter(const PhaseNoiseDensityVector& new_pn) {
    const auto f_dss = dds.get_lo_freq(channel);

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

            std::size_t k1 = std::max(1u, static_cast<std::size_t>(sci::ceil(f_lo_used / df).eval()));
            std::size_t k2 = std::min(n_bins - 1u, static_cast<std::size_t>(sci::floor(f_hi_used / df).eval()));

            if (k2 <= k1) {
                k1 = std::max(1u, k1);
                k2 = std::min(n_bins - 1u, std::max(k1 + 1, k2));
            }

            phase_jitter = sci::sqrt(sci::trapz(new_pn.begin() + k1, new_pn.begin() + k2 + 1, df));
        }

        time_jitter = phase_jitter / (2.0f * sci::pi<Phase> * f_dss);
    }
}

void PhaseNoiseAnalyzer::start_acquisition() {
    if (! acquisition_started) {
        acq_thread = std::thread{&PhaseNoiseAnalyzer::acquisition_thread, this};
        acq_thread.detach();
    }
}

void PhaseNoiseAnalyzer::acquisition_thread() {
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