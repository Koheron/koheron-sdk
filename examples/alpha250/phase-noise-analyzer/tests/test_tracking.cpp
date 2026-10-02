#include "../phase-noise-analyzer.hpp"
#include "server/drivers/dma-s2mm.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/config_manager.hpp"
#include "server/network/serializer_deserializer.hpp"
#include <cassert>
#include <cmath>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>

int main(int argc, char** argv) {
    auto& cfg = services::require<rt::ConfigManager>();
    cfg.set("PhaseNoiseAnalyzer", "cic_rate", 8192u);
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[0]", 10e6 + .637);
    cfg.set("PhaseNoiseAnalyzer", "dds_freq[1]", 12e6 + .125);
    cfg.set("PhaseNoiseAnalyzer", "tracking_enabled", true);
    cfg.set("PhaseNoiseAnalyzer", "tracking_max_step", .01f);
    cfg.set("PhaseNoiseAnalyzer", "tracking_max_correction", 1.f);
    auto& dma = rt::get_driver<DmaS2MM>();
    auto& ram = hw::get_memory<mem::ram>();
    ram.amplitude = .1;
    ram.carrier_frequency = {10e6 + .597, 12e6 + .165};
    PhaseNoiseAnalyzer analyzer;
    unsigned waits = 1;
    dma.wait_until(waits);
    auto acquire = [&](bool success = true) { dma.complete(success); dma.wait_until(++waits); };
    auto boundary_change = [&](auto action) {
        auto change = std::async(std::launch::async, action);
        for (int i = 0; i < 20 && change.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready; ++i)
            acquire();
        assert(change.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
        change.get();
    };
    auto initial = analyzer.get_tracking_parameters();
    assert(std::get<0>(initial));
    assert(std::abs(std::get<2>(initial) - 12207.03125 / (50 * 32768)) < 1e-12);
    assert(std::abs(std::get<5>(initial) - (10e6 + .637)) < 1e-6);
    const double nominal0 = std::get<5>(initial), nominal1 = std::get<6>(initial);
    for (int i = 0; i < 4; ++i) acquire();
    assert(std::get<7>(analyzer.get_tracking_parameters()) == 0.);

    const auto mid_capture_writes = hw::dds_writes_during_transfer.load();
    double previous = nominal0;
    for (int i = 0; i < 50; ++i) {
        acquire();
        const double lo = std::get<5>(analyzer.get_parameters());
        assert(lo <= previous + 1e-6); // positive phase slope requires lower LO
        assert(std::abs(lo - previous) <= .01 + 1e-6);
        assert(std::get<6>(analyzer.get_parameters()) == nominal1);
        previous = lo;
    }
    // The finite-window PM projects a small component onto the slope fit.
    std::cerr << "ADC0 residual frequency error: " << previous - ram.carrier_frequency[0] << " Hz\n";
    assert(std::abs(previous - ram.carrier_frequency[0]) < 1e-4);
    assert(hw::dds_writes_during_transfer.load() == mid_capture_writes);
    const auto settled = analyzer.get_tracking_parameters();
    assert(std::get<11>(settled) && !std::get<12>(settled));
    assert(std::abs(std::get<7>(settled) + .04) < 1e-4);
    assert(std::abs(std::get<9>(settled)) < 1e-5);
    const double settled_lo0 = previous;
    // The bin-64 PM and its integrated power remain present while tracking.
    const auto pn = analyzer.get_phase_noise();
    double power = 0.;
    for (unsigned i = 62; i <= 66; ++i) power += pn[i].eval() * 12207.03125 / 32768;
    assert(std::abs(power / .005 - 1) < .005);
    analyzer.save_config();
    assert(cfg.get<double>("PhaseNoiseAnalyzer", "dds_freq[0]") == nominal0);
    assert(cfg.get<bool>("PhaseNoiseAnalyzer", "tracking_enabled"));
    if (argc == 2) {
        std::pmr::vector<unsigned char> bytes;
        net::CommandBuilder serializer;
        serializer.reset_into(bytes); serializer.push(settled);
        assert(bytes.size() == 83); // bool + 10 doubles + 2 bools
        std::ofstream(argv[1], std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    analyzer.set_channel(1);
    assert(!std::get<11>(analyzer.get_tracking_parameters()));
    for (int i = 0; i < 2; ++i) acquire();
    previous = nominal1;
    for (int i = 0; i < 50; ++i) {
        acquire();
        const double lo = std::get<6>(analyzer.get_parameters());
        assert(lo >= previous - 1e-6);
        assert(std::abs(lo - previous) <= .01 + 1e-6);
        assert(std::get<5>(analyzer.get_parameters()) == settled_lo0);
        previous = lo;
    }
    assert(std::abs(previous - ram.carrier_frequency[1]) < 1e-4);
    assert(std::get<12>(analyzer.get_tracking_parameters()));

    ram.carrier_frequency[1] = nominal1 + 2;
    boundary_change([&] { analyzer.set_tracking_max_correction(.05f); });
    for (int i = 0; i < 20; ++i) acquire();
    assert(std::abs(std::get<8>(analyzer.get_tracking_parameters()) - .05) < 1e-6);
    assert(!std::get<12>(analyzer.get_tracking_parameters()));
    boundary_change([&] { analyzer.set_tracking_max_correction(.005f); });
    assert(std::abs(std::get<8>(analyzer.get_tracking_parameters()) - .005) < 1e-6);
    assert(analyzer.get_phase_noise()[64].eval() == 0.f);
    auto valid = analyzer.get_tracking_parameters();
    for (float value : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        analyzer.set_tracking_bandwidth(value);
        analyzer.set_tracking_max_step(value);
        analyzer.set_tracking_max_correction(value);
    }
    auto unchanged = analyzer.get_tracking_parameters();
    assert(std::get<1>(unchanged) == std::get<1>(valid));
    assert(std::get<3>(unchanged) == std::get<3>(valid));
    assert(std::get<4>(unchanged) == std::get<4>(valid));
    assert(std::get<8>(unchanged) == std::get<8>(valid));
    analyzer.set_tracking_bandwidth(0.f);
    const double paused = std::get<6>(analyzer.get_parameters());
    acquire(); acquire();
    assert(std::get<6>(analyzer.get_parameters()) == paused);
    assert(!std::get<12>(analyzer.get_tracking_parameters()));
    acquire(false);
    assert(std::isnan(std::get<10>(analyzer.get_tracking_parameters())));
    assert(!std::get<12>(analyzer.get_tracking_parameters()));

    boundary_change([&] { analyzer.set_cic_rate(4096); });
    assert(std::get<1>(analyzer.get_parameters()).eval() == 24414.0625f);
    analyzer.set_tracking_bandwidth(.1f);
    assert(std::abs(std::get<2>(analyzer.get_tracking_parameters()) - 24414.0625 / (50 * 32768)) < 1e-12);

    boundary_change([&] { analyzer.set_tracking_enabled(false); });
    assert(!std::get<0>(analyzer.get_tracking_parameters()));
    assert(std::get<5>(analyzer.get_parameters()) == nominal0);
    assert(std::get<6>(analyzer.get_parameters()) == nominal1);
    assert(std::get<7>(analyzer.get_tracking_parameters()) == 0.);
    assert(std::get<8>(analyzer.get_tracking_parameters()) == 0.);
    analyzer.set_local_oscillator(1, 0.);
    ram.carrier_frequency[1] = std::numeric_limits<double>::quiet_NaN();
    boundary_change([&] { analyzer.set_tracking_enabled(true); });
    for (int i = 0; i < 5; ++i) acquire();
    assert(std::get<6>(analyzer.get_parameters()) == 0.);
    assert(!std::get<12>(analyzer.get_tracking_parameters()));
    dma.cancel();
    std::cout << "Closed-loop tracking, both signs/channels, bounds, lock, PM preservation and wire format passed\n";
}
