#include "../phase-noise-analyzer.hpp"
#include "server/drivers/dma-s2mm.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/config_manager.hpp"
#include <cassert>
#include <cmath>
#include <future>
#include <iostream>

int main(int argc, char**) {
    rt::get_driver<ClockGenerator>().switchable = true;
    auto& cfg = services::require<rt::ConfigManager>();
    cfg.set("PhaseNoiseAnalyzer", "cic_rate", 100u);
    cfg.set("PhaseNoiseAnalyzer", "fft_navg", 3u);
    cfg.set("PhaseNoiseAnalyzer", "phase_precision", 8u);
    if (argc > 1) cfg.set("PhaseNoiseAnalyzer", "sampling_frequency", 200000000u);
    auto& ram = hw::get_memory<mem::ram>();
    ram.carrier_frequency = {10e6, 10e6};
    ram.amplitude = .1;
    auto& dma = rt::get_driver<DmaS2MM>();
    PhaseNoiseAnalyzer analyzer;
    assert(analyzer.get_sampling_frequency() == (argc > 1 ? 200000000u : 250000000u));
    unsigned waits = 1;
    dma.wait_until(waits);
    const auto acquire = [&] { dma.complete(true); dma.wait_until(++waits); };
    const auto change = [&](uint32_t rate) {
        auto result = std::async(std::launch::async, [&] { return analyzer.set_sampling_frequency(rate); });
        for (unsigned i = 0; i < 30 && result.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready; ++i)
            acquire();
        assert(result.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
        return result.get();
    };
    const auto verify_spectrum = [&] {
        for (unsigned i = 0; i < 10; ++i) acquire();
        const auto snapshot = analyzer.get_spectrum_snapshot();
        assert(std::get<1>(snapshot) == 1);
        const double fs = analyzer.get_sampling_frequency() / 200.;
        assert(std::get<3>(snapshot) == fs);
        assert(std::get<5>(snapshot) == 100);
        assert(std::get<2>(snapshot) == 8);
        double power = 0;
        for (unsigned bin = 62; bin <= 66; ++bin) power += std::get<16>(snapshot)[bin].eval() * fs / 32768;
        assert(std::abs(power / .005 - 1) < .005);
        assert(std::abs(std::get<5>(analyzer.get_parameters()) - 10e6) < 1e-6);
    };
    verify_spectrum();
    assert(!change(240000000));
    const auto writes_in_capture = hw::dds_writes_during_transfer.load();
    for (uint32_t rate : {200000000u, 250000000u, 200000000u, 250000000u}) {
        const bool different = rate != analyzer.get_sampling_frequency();
        assert(change(rate));
        assert(analyzer.get_sampling_frequency() == rate);
        if (different) {
            assert(std::get<1>(analyzer.get_spectrum_snapshot()) == 0);
            assert(std::get<0>(analyzer.get_average_status()) == 0);
        }
        verify_spectrum();
        const auto average = analyzer.get_average_status();
        assert(change(rate)); // A repeated selection preserves a valid average.
        assert(analyzer.get_average_status() == average);
        assert(std::get<1>(analyzer.get_spectrum_snapshot()) == 1);
    }
    assert(hw::dds_writes_during_transfer.load() == writes_in_capture);
    analyzer.set_local_oscillator(1, 110e6);
    verify_spectrum();
    const auto lo_limited_average = analyzer.get_average_status();
    assert(!change(200000000));
    assert(analyzer.get_sampling_frequency() == 250000000);
    assert(analyzer.get_average_status() == lo_limited_average);
    analyzer.set_local_oscillator(1, 10e6);
    verify_spectrum();
    auto& generator = rt::get_driver<PhaseModulator>();
    generator.compatible = false;
    const auto average = analyzer.get_average_status();
    assert(!change(200000000));
    assert(analyzer.get_sampling_frequency() == 250000000);
    assert(analyzer.get_average_status() == average);
    generator.compatible = true;
    generator.fail_change = true;
    assert(!change(200000000));
    assert(analyzer.get_sampling_frequency() == 250000000);
    verify_spectrum();
    generator.fail_change = false;
    assert(change(200000000));
    analyzer.save_config();
    assert(cfg.get<uint32_t>("PhaseNoiseAnalyzer", "sampling_frequency") == 200000000);
    dma.cancel();
    std::cout << "Sample-clock transitions, PSD calibration, epoch boundaries and saved selection passed\n";
}
