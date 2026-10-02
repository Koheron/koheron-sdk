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
    auto& cfg = services::require<rt::ConfigManager>();
    cfg.set("PhaseNoiseAnalyzer", "cic_rate", 16u);
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[0]", 10e6 + 0.637);
    auto& dma = rt::get_driver<DmaS2MM>();
    auto& ram = hw::get_memory<mem::ram>();
    auto& ctl = hw::get_memory<mem::control>();
    ram.drift = 3.141592653589793 / 2048; // one count/sample, exactly representable in raw DMA
    PhaseNoiseAnalyzer analyzer;
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
    });
    assert(readers.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    readers.get();
    for (int i = 0; i < 4; ++i) acquire(); // LO settling
    assert(analyzer.get_phase()[100].eval() == 0.f);
    acquire();
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

    analyzer.set_channel(1);
    assert(analyzer.get_phase()[100].eval() == 0.f);
    assert(std::isnan(std::get<0>(analyzer.get_jitter()).eval()));
    for (int i = 0; i < 2; ++i) acquire();
    assert(analyzer.get_phase()[100].eval() == 0.f);
    acquire();
    const auto reads = ram.reads.load();
    acquire(false);
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

    // Stop the simulated DMA before the analyzer's destructor joins acquisition.
    dma.cancel();
    std::cout << "Production acquisition, precision, drift, settling, failure and averaging checks passed\n";
}
