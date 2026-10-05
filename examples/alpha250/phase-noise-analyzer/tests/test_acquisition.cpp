// Compile the production analyzer/DDS against controlled hardware dependencies.
#include "../phase-noise-analyzer.hpp"
#include "server/drivers/dma-s2mm.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/config_manager.hpp"
#include <cassert>
#include <cmath>
#include <future>
#include <iostream>
#include <limits>

int main() {
    // Compare the cached production estimator with the independent library
    // implementation, including DC, Nyquist, broadband noise and plan reuse.
    using Phase = scicpp::units::radian<float>;
    // A large integer drift ramp must not discard its small PM before FFT.
    // Compare with the same quantized PM without the ramp, using the original
    // independent float detrending helper as the reference.
    std::array<int32_t, 65536> raw{};
    std::array<Phase, 65536> truth{}, converted{};
    const Phase fine_step{float(scicpp::pi<double> / 500000)};
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const int32_t pm = int32_t(std::lrint(.001 / double(fine_step.eval()) *
            std::sin(2 * scicpp::pi<double> * 64 * double(i) / 32768)));
        raw[i] = int32_t(int64_t(i) * 16000 - 524288000 + pm);
        truth[i] = fine_step * float(pm);
    }
    const auto expected_residual = detrended_phase_prefix<65536>(truth);
    const auto trend = phase_noise::fit_raw_phase_prefix<65536>(raw);
    const auto residual = phase_noise::detrended_raw_phase_prefix<65536>(raw, trend, fine_step);
    convert_relative_phase(raw, converted, fine_step);
    const auto float_first = detrended_phase_prefix<65536>(converted);
    double old_error = 0;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        assert(std::abs(residual[i].eval() - expected_residual[i].eval()) < 1e-8f);
        old_error += std::pow(double(float_first[i].eval() - expected_residual[i].eval()), 2);
    }
    assert(std::sqrt(old_error / double(raw.size())) > 1e-4);
    // Extreme half-block steps exercise the integer fit's accumulation bound.
    std::fill(raw.begin(), raw.begin() + 32768, std::numeric_limits<int32_t>::min());
    std::fill(raw.begin() + 32768, raw.end(), std::numeric_limits<int32_t>::max());
    for (const double slope : {98304.0, -98304.0}) {
        const auto extreme = phase_noise::fit_raw_phase_prefix<65536>(raw);
        assert(extreme.anchor + extreme.mean == -0.5);
        assert(std::abs(extreme.slope - slope) < 1e-9);
        for (auto& value : raw) value = int32_t(-int64_t(value) - 1);
    }
    std::array<Phase, 65536> signal{};
    uint32_t random = 1;
    for (std::size_t i = 0; i < signal.size(); ++i) {
        random = 1664525u * random + 1013904223u;
        signal[i] = Phase{float(.1 * std::sin(2 * scicpp::pi<double> * 64 * double(i) / 32768) +
            .01 * double(random) / double(UINT32_MAX) + (i % 2 ? -.02 : .02))};
    }
    phase_noise::WelchSpectrum<32768> cached;
    scicpp::signal::Spectrum<float> reference;
    reference.window(scicpp::signal::windows::hann<float>(32768));
    for (std::size_t i = 0; i < raw.size(); ++i)
        raw[i] = int32_t(int64_t(i) * 16000 - 524288000 +
                         int32_t(std::lrint(signal[i].eval() / fine_step.eval())));
    const auto raw_trend = phase_noise::fit_raw_phase_prefix<65536>(raw);
    const auto raw_residual = phase_noise::detrended_raw_phase_prefix<65536>(raw, raw_trend, fine_step);
    for (float frequency : {3125000.f, 6250000.f, 3125000.f}) {
        reference.fs(frequency);
        const auto expected = reference.welch<scicpp::signal::SpectrumScaling::DENSITY, false>(signal);
        const auto actual = cached.density(signal, scicpp::units::frequency<float>{frequency});
        const auto peak = std::max_element(expected.begin(), expected.end())->eval();
        assert(actual.size() == expected.size());
        for (std::size_t i = 0; i < actual.size(); ++i)
            assert(std::abs(actual[i].eval() - expected[i].eval()) < 2e-5f * peak);
        const auto raw_expected =
            reference.welch<scicpp::signal::SpectrumScaling::DENSITY, false>(raw_residual);
        const auto raw_actual = cached.density(raw, raw_trend, fine_step,
                                              scicpp::units::frequency<float>{frequency});
        const auto raw_peak = std::max_element(raw_expected.begin(), raw_expected.end())->eval();
        for (std::size_t i = 0; i < raw_actual.size(); ++i)
            assert(std::abs(raw_actual[i].eval() - raw_expected[i].eval()) < 2e-5f * raw_peak);
    }

    auto& cfg = services::require<rt::ConfigManager>();
    cfg.set("PhaseNoiseAnalyzer", "cic_rate", 16u);
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[0]", 10e6 + 0.637);
    auto& dma = rt::get_driver<DmaS2MM>();
    auto& ram = hw::get_memory<mem::ram>();
    auto& ctl = hw::get_memory<mem::control>();
    ram.drift = 3.141592653589793 / 2048; // one count/sample, exactly representable in raw DMA
    PhaseNoiseAnalyzer analyzer;
    assert((analyzer.get_average_status() == std::tuple{0u, 1u}));
    unsigned waits = 1;
    dma.wait_until(waits);
    auto acquire = [&](bool success = true) {
        dma.complete(success);
        dma.wait_until(++waits); // next wait starts after processing/publication
    };
    auto parameters = analyzer.get_parameters();
    assert(!std::get<0>(analyzer.get_tracking_parameters())); // legacy configs remain opt-in
    assert(std::get<0>(parameters) == 16385);
    assert(std::get<1>(parameters).eval() == 6250000.f);
    constexpr double lsb = 200e6 / (uint64_t{1} << 48);
    assert(std::abs(std::get<5>(parameters) - (10e6 + 0.637)) <= lsb / 2);
    analyzer.save_config();
    assert(cfg.get<double>("PhaseNoiseAnalyzer", "dds_freq[0]") == std::get<5>(parameters));

    // Snapshot readers must finish while DMA remains blocked.
    auto readers = std::async(std::launch::async, [&] {
        analyzer.get_parameters(); analyzer.get_phase(); analyzer.get_phase_noise();
        analyzer.get_measurements(1); analyzer.get_jitter();
        analyzer.get_average_status();
    });
    assert(readers.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    readers.get();
    for (int i = 0; i < 4; ++i) acquire(); // LO settling
    assert((analyzer.get_average_status() == std::tuple{0u, 1u}));
    assert(analyzer.get_phase()[100].eval() == 0.f);
    acquire();
    assert((analyzer.get_average_status() == std::tuple{1u, 1u}));
    const auto phase = analyzer.get_phase();
    assert(phase.front().eval() == 0.f);
    assert(std::abs(phase.back().eval() - ram.drift * 65535) < .01);
    const auto pn = analyzer.get_phase_noise();
    // Integrated bin-64 PM, after Hann leakage. Detrending must retain known power.
    double tone_power = 0;
    for (unsigned i = 62; i <= 66; ++i) tone_power += pn[i].eval() * 6250000 / 32768;
    assert(std::abs(tone_power / (.1 * .1 / 2) - 1) < .005);
    assert(pn[1].eval() < pn[64].eval() * .001); // affine drift rejected

    const auto writes = ctl.writes.load();
    for (double freq : {std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity(), -1., 100e6 + 1})
        analyzer.set_local_oscillator(0, freq);
    analyzer.set_local_oscillator(2, 1e6);
    analyzer.set_channel(2);
    analyzer.set_cic_rate(0);
    analyzer.set_analyzer_mode(2);
    analyzer.set_interferometer_delay(-1.f);
    assert(ctl.writes.load() == writes);
    assert(analyzer.get_phase_noise() == pn);

    // Progress follows the actual ring, including retained samples on resize.
    analyzer.set_fft_navg(4);
    assert((analyzer.get_average_status() == std::tuple{1u, 4u}));
    for (uint32_t count = 2; count <= 4; ++count) {
        acquire();
        assert((analyzer.get_average_status() == std::tuple{count, 4u}));
    }
    acquire();
    assert((analyzer.get_average_status() == std::tuple{4u, 4u}));
    analyzer.set_fft_navg(2);
    assert((analyzer.get_average_status() == std::tuple{2u, 2u}));
    analyzer.set_fft_navg(4);
    assert((analyzer.get_average_status() == std::tuple{2u, 4u}));

    analyzer.set_channel(1);
    assert((analyzer.get_average_status() == std::tuple{0u, 4u}));
    assert(analyzer.get_phase()[100].eval() == 0.f);
    assert(std::isnan(std::get<0>(analyzer.get_jitter()).eval()));
    for (int i = 0; i < 2; ++i) acquire();
    assert(analyzer.get_phase()[100].eval() == 0.f);
    acquire();
    const auto reads = ram.reads.load();
    acquire(false);
    assert((analyzer.get_average_status() == std::tuple{0u, 4u}));
    assert(ram.reads.load() == reads); // failed DMA cannot read stale RAM
    assert(analyzer.get_phase()[100].eval() == 0.f);
    assert(std::isnan(std::get<0>(analyzer.get_jitter()).eval()));
    for (int i = 0; i < 3; ++i) acquire();
    assert(analyzer.get_phase_noise()[64].eval() > 0.f);

    analyzer.set_fft_navg(2); acquire();
    analyzer.set_fft_navg(1);
    ram.amplitude = 0; ram.drift = 0;
    acquire();
    analyzer.set_fft_navg(2); acquire();
    assert(analyzer.get_phase_noise()[64].eval() == 0.f); // no old unaveraged history

    ram.amplitude = .1;
    analyzer.set_fft_navg(1000);
    assert(std::get<4>(analyzer.get_parameters()) == 100);
    analyzer.set_fft_navg(0);
    assert(std::get<4>(analyzer.get_parameters()) == 1);
    acquire();
    const auto rf = analyzer.get_phase_noise()[64].eval();
    analyzer.set_interferometer_delay(1e-6f);
    analyzer.set_analyzer_mode(1);
    assert(analyzer.get_phase_noise()[64].eval() == 0.f);
    acquire();
    const double response = .25 / std::pow(std::sin(3.141592653589793 * 64 * 6250000 / 32768 * 1e-6), 2);
    assert(std::abs(analyzer.get_phase_noise()[64].eval() / rf / response - 1) < 1e-5);
    analyzer.set_analyzer_mode(0);
    acquire();
    assert(std::abs(analyzer.get_phase_noise()[64].eval() / rf - 1) < 1e-5);

    // A rate change waits for the transfer, and clears the prior calibration/results.
    auto rate_change = std::async(std::launch::async, [&] { analyzer.set_cic_rate(32); });
    for (int i = 0; i < 10 && rate_change.wait_for(std::chrono::milliseconds(20)) != std::future_status::ready; ++i)
        acquire();
    assert(rate_change.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    rate_change.get();
    assert(std::get<1>(analyzer.get_parameters()).eval() == 3125000.f);
    assert(analyzer.get_phase()[100].eval() == 0.f);

    // Spectrum snapshots must also finish while an exclusive settings/processing
    // lock is held. A blocked calibration gives this test a deterministic writer.
    auto& adc = rt::get_driver<Ltc2157>();
    adc.block_conversion();
    auto channel_change = std::async(std::launch::async, [&] { analyzer.set_channel(1); });
    adc.wait_for_conversion();
    auto spectrum_read = std::async(std::launch::async, [&] { return analyzer.get_phase_noise(); });
    const auto snapshot_status = spectrum_read.wait_for(std::chrono::seconds(1));
    adc.release_conversion();
    channel_change.get();
    const auto invalidated = spectrum_read.get();
    assert(snapshot_status == std::future_status::ready);
    assert(invalidated.size() == 16385);
    assert(std::all_of(invalidated.begin(), invalidated.end(), [](auto value) { return value.eval() == 0.f; }));

    // Every retained-bit choice must preserve the calibrated PM power. Reject
    // stale packet scales and overrange data instead of publishing clipped PSD.
    ram.origin = 0; ram.drift = .001; ram.amplitude = .1;
    for (uint32_t bits = 0; bits <= 8; ++bits) {
        assert(analyzer.set_phase_precision(bits));
        for (unsigned i = 0; i < 5; ++i) acquire();
        const auto status = analyzer.get_precision_status();
        assert(std::get<0>(status) == bits && std::get<1>(status) == bits && std::get<3>(status) == 1);
        assert(std::abs(std::get<2>(status) / (scicpp::pi<double> / 2048 * std::exp2(-double(bits))) - 1) < 1e-6);
        const auto precise_psd = analyzer.get_phase_noise();
        double precise_power = 0;
        for (unsigned i = 62; i <= 66; ++i) precise_power += precise_psd[i].eval() * 3125000 / 32768;
        assert(std::abs(precise_power / (.1 * .1 / 2) - 1) < .005);
        assert(std::get<1>(analyzer.get_phase_snapshot()) == bits);
    }
    assert(!analyzer.set_phase_precision(9));
    analyzer.save_config();
    assert(cfg.get<uint32_t>("PhaseNoiseAnalyzer", "phase_precision") == 8);
    hw::injected_precision.store(7);
    acquire();
    assert(std::get<3>(analyzer.get_precision_status()) == 0);
    assert(!std::get<3>(analyzer.get_phase_snapshot()));
    hw::injected_precision.store(32);
    hw::injected_packet_flags.store(0x10);
    acquire();
    assert(std::get<3>(analyzer.get_precision_status()) == 2);
    assert(std::get<5>(analyzer.get_precision_status()) == 1);
    assert(analyzer.get_phase_noise()[64].eval() == 0.f);
    hw::injected_packet_flags.store(0);
    acquire();
    assert(std::get<3>(analyzer.get_precision_status()) == 1);

    // Stop the simulated DMA before the analyzer's destructor joins acquisition.
    dma.cancel();
    std::cout << "Production acquisition, precision, drift, settling, failure and averaging checks passed\n";
}
