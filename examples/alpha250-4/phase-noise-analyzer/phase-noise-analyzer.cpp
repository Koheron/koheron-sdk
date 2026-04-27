/// PhaseNoiseAnalyzer driver
///
/// (c) Koheron

#include "./phase-noise-analyzer.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/config_manager.hpp"
#include "server/drivers/dma-s2mm.hpp"
#include "boards/alpha250-4/drivers/ltc2157.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
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

    log<INFO>("PhaseNoiseAnalyzer: Starting...");

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
    start_acquisition();
}

void PhaseNoiseAnalyzer::save_config() {
    cfg.set("PhaseNoiseAnalyzer", "channel", channel);
    cfg.set("PhaseNoiseAnalyzer", "fft_navg", fft_navg);
    cfg.set("PhaseNoiseAnalyzer", "cic_rate", cic_rate);
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[X]", dds.get_dds_freq(0));
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[Y]", dds.get_dds_freq(2));
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[XY]", dds.get_dds_freq(0));
    cfg.set("PhaseNoiseAnalyzer", "analyzer_mode", analyzer_mode);
    cfg.set("PhaseNoiseAnalyzer", "interferometer_delay", interferometer_delay.eval());
    cfg.save();
}

void PhaseNoiseAnalyzer::set_local_oscillator(uint32_t channel, double freq_hz) {
    dirty_cnt = 4;

    if (channel == InputChannel::X) {
        dds.set_dds_freq(0, freq_hz);
        dds.set_dds_freq(1, freq_hz);
    } else if (channel == InputChannel::Y) {
        dds.set_dds_freq(2, freq_hz);
        dds.set_dds_freq(3, freq_hz);
    } else if (channel == InputChannel::XY) {
        dds.set_dds_freq(0, freq_hz);
        dds.set_dds_freq(1, freq_hz);
        dds.set_dds_freq(2, freq_hz);
        dds.set_dds_freq(3, freq_hz);
    } else {
        // TODO Crossed-channel ch0 x ch1
        logf<ERROR>("PhaseNoiseAnalyzer::set_local_oscillator: Invalid channel {}\n", channel);
    }

    averager.clear();
}

void PhaseNoiseAnalyzer::set_cic_rate(uint32_t rate) {
    if (rate < prm::cic_decimation_rate_min ||
        rate > prm::cic_decimation_rate_max) {
        log<ERROR>("PhaseNoiseAnalyzer: CIC rate out of range\n");
        return;
    }

    std::scoped_lock lk(dma_mtx); // block until any DMA transfer finishes

    cic_rate = rate;
    fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
    dma_transfer_duration = prm::n_pts / fs;
    logf("DMA transfer duration = {} s\n", dma_transfer_duration.eval());

    update_interferometer_transfer_function();
    spectrum.fs(fs);
    averager.clear();
    dirty_cnt = 2;
    ctl.write<reg::cic_rate>(cic_rate);
}

void PhaseNoiseAnalyzer::set_channel(uint32_t chan) {
    if (chan != InputChannel::X && chan != InputChannel::Y && chan != InputChannel::XY) {
        log<ERROR>("PhaseNoiseAnalyzer: Invalid channel\n");
        return;
    }

    std::scoped_lock lk(dma_mtx);

    channel = chan;

    if (chan == InputChannel::X || chan == InputChannel::Y) {
        axis_stream_mux.select_input(channel);
    } else { // XY
        axis_stream_mux.select_input(0);
    }

    averager.clear();
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

    static constexpr uint32_t n_chunks_to_read = data_size / samples_per_chunk;

    while (write_count.load(std::memory_order_acquire) < n_chunks_to_read) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const uint64_t write_count_snapshot = write_count.load(std::memory_order_acquire);
    const uint64_t first_chunk = write_count_snapshot - n_chunks_to_read;
    const uint32_t first_idx = first_chunk % n_chunks;
    const uint32_t byte_offset = x_byte_offset + first_idx * chunk_bytes;

    auto samples = std::array<int32_t, data_size>{};
    auto& ram = hw::get_memory<mem::ram>();

    {
        std::scoped_lock lk(dma_mtx);
        samples = ram.read_reg_array<int32_t, data_size>(byte_offset);
    }

    phase_x = samples * calib_factor;
    return phase_x;
}

PhaseNoiseAnalyzer::PhaseDataArray PhaseNoiseAnalyzer::get_phase_y() const {
    std::shared_lock lk(data_mtx);
    return phase_y;
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
        logf<WARNING>("PhaseNoiseAnalyzer: Invalid mode {}\n", mode);
        return;
    }

    analyzer_mode = mode;
}

void PhaseNoiseAnalyzer::set_interferometer_delay(float delay_s) {
    interferometer_delay = Time(delay_s);
    logf("PhaseNoiseAnalyzer: Interferometer delay set to {} ns\n",
         interferometer_delay.eval() * 1E9f);

    update_interferometer_transfer_function();
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

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[X]")) {
        set_local_oscillator(InputChannel::X, cfg.get<uint32_t>("PhaseNoiseAnalyzer", "dds_freqX]"));
    } else {
        set_local_oscillator(InputChannel::X, 10E6);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[Y]")) {
        set_local_oscillator(InputChannel::Y, cfg.get<uint32_t>("PhaseNoiseAnalyzer", "dds_freq[Y]"));
    } else {
        set_local_oscillator(InputChannel::Y, 10E6);
    }

    if (cfg.has("PhaseNoiseAnalyzer", "dds_freq[XY]")) {
        set_local_oscillator(InputChannel::XY, cfg.get<uint32_t>("PhaseNoiseAnalyzer", "dds_freq[XY]"));
    } else {
        set_local_oscillator(InputChannel::XY, 10E6);
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
    ctl.set_bit<reg::cordic, 1>();
    ctl.clear_bit<reg::cordic, 1>();
}

// auto PhaseNoiseAnalyzer::read_dma() {
//     std::scoped_lock lk(dma_mtx);

//     constexpr uint32_t bytes_per_sample = sizeof(int32_t);
//     constexpr uint32_t transfer_size = prm::n_pts * bytes_per_sample;

//     // logf("PhaseNoiseAnalyzer::read_dma(): bytes_per_sample = {}, transfer_size = {}\n", bytes_per_sample, transfer_size);

//     uint32_t byte_offset = 0;
//     while (byte_offset < transfer_size) {
//         const uint32_t remaining_bytes = transfer_size - byte_offset;
//         const uint32_t chunk_samples = std::min(dma_chunk_beats_max, remaining_bytes / bytes_per_sample);
//         const uint32_t chunk_bytes = chunk_samples * bytes_per_sample;
//         const float chunk_duration = static_cast<float>(chunk_samples) / fs.eval();

//         axis_stream_mux.set_packet_length(chunk_samples);
//         dma.start_transfer(hw::Memory<mem::ram>::phys_addr + byte_offset, chunk_bytes);
//         axis_stream_mux.trigger();
//         dma.wait_for_transfer(chunk_duration);

//         byte_offset += chunk_bytes;
//     }

//     // logf("PhaseNoiseAnalyzer::read_dma(): byte_offset = {}\n", byte_offset);
//     auto& ram = hw::get_memory<mem::ram>();
//     return ram.read_array<int32_t, data_size, read_offset>();
// }

// auto PhaseNoiseAnalyzer::read_dma_xy() {
//     std::scoped_lock lk(dma_mtx);

//     constexpr uint32_t bytes_per_sample = sizeof(int32_t);
//     constexpr uint32_t transfer_size = prm::n_pts * bytes_per_sample;
//     constexpr uint32_t x_byte_offset = 0;
//     constexpr uint32_t y_byte_offset = transfer_size;
//     constexpr uint32_t y_read_offset = prm::n_pts + read_offset;

//     uint32_t byte_offset = 0;
//     while (byte_offset < transfer_size) {
//         const uint32_t remaining_bytes = transfer_size - byte_offset;
//         const uint32_t chunk_samples = std::min(dma_chunk_beats_max, remaining_bytes / bytes_per_sample);
//         const uint32_t chunk_bytes = chunk_samples * bytes_per_sample;
//         const float chunk_duration = static_cast<float>(chunk_samples) / fs.eval();

//         axis_stream_mux.select_input(InputChannel::X);
//         axis_stream_mux.set_packet_length(chunk_samples);
//         axis_stream_mux.trigger();
//         dma.start_transfer(hw::Memory<mem::ram>::phys_addr + x_byte_offset + byte_offset, chunk_bytes);
//         dma.wait_for_transfer(chunk_duration);

//         axis_stream_mux.select_input(InputChannel::Y);
//         axis_stream_mux.set_packet_length(chunk_samples);
//         axis_stream_mux.trigger();
//         dma.start_transfer(hw::Memory<mem::ram>::phys_addr + y_byte_offset + byte_offset, chunk_bytes);
//         dma.wait_for_transfer(chunk_duration);

//         byte_offset += chunk_bytes;
//     }

//     auto& ram = hw::get_memory<mem::ram>();
//     return std::tuple{
//         ram.read_array<int32_t, data_size, read_offset>(),
//         ram.read_array<int32_t, data_size, y_read_offset>()
//     };
// }

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

    // TODO Only ADC0 is used for now. Use proper transfer function for ADC1 also.
    const double Hinv = sci::polynomial::polyval(dds.get_dds_freq(channel),
                                                 ltc2157.tf_polynomial<double>(0, channel));
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

auto PhaseNoiseAnalyzer::compute_crossed_phase_noise(PhaseDataArray& new_phase_x, PhaseDataArray& new_phase_y) {
    auto phase_psd = sci::sqrt(sci::norm(spectrum.csd<sig::DENSITY, false>(new_phase_x, new_phase_y)));

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
        axis_stream_mux.select_input(channel);
        acq_thread = std::thread{&PhaseNoiseAnalyzer::acquisition_thread, this};
        acq_thread.detach();
    }
}

void PhaseNoiseAnalyzer::acquisition_thread() {
    constexpr auto dma_phys_addr = hw::Memory<mem::ram>::phys_addr;
    constexpr auto dma_x_start_addr = dma_phys_addr + x_byte_offset;
    constexpr auto dma_y_start_addr = dma_phys_addr + y_byte_offset;

    const float chunk_duration = static_cast<float>(samples_per_chunk) / fs.eval();

    uint32_t idx = 0;
    acquisition_started = true;

    while (acquisition_started) {
        uint32_t byte_offset = idx * chunk_bytes;

        axis_stream_mux.select_input(InputChannel::X);
        axis_stream_mux.set_packet_length(samples_per_chunk);

        {
            std::scoped_lock lk(dma_mtx);
            dma.start_transfer(dma_x_start_addr + byte_offset, chunk_bytes);
            axis_stream_mux.trigger();
            dma.wait_for_transfer(chunk_duration);
        }

        axis_stream_mux.select_input(InputChannel::Y);
        axis_stream_mux.set_packet_length(samples_per_chunk);

        {
            std::scoped_lock lk(dma_mtx);
            dma.start_transfer(dma_y_start_addr + byte_offset, chunk_bytes);
            axis_stream_mux.trigger();
            dma.wait_for_transfer(chunk_duration);
        }

        write_count.fetch_add(1, std::memory_order_release);
        write_idx.store(idx, std::memory_order_release);
        idx = (idx + 1) % n_chunks;
    }
}
