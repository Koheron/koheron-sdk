// Exercise production settings against the descriptor-ring hardware stub.
#include "../phase-noise-analyzer.hpp"
#include "simulated_dma.hpp"
#include "server/runtime/config_manager.hpp"
#include "server/runtime/services.hpp"
#include <cassert>
#include <iostream>
#include <memory>

int main(int argc, char**) {
    auto& clock = rt::get_driver<ClockGenerator>();
    clock.switchable = true;
    auto& cfg = services::require<rt::ConfigManager>();
    if (argc > 1) cfg.set("PhaseNoiseAnalyzer", "sampling_frequency", 200000000u);
    PhaseNoiseAnalyzer analyzer;
    assert(analyzer.get_sampling_frequency() == (argc > 1 ? 200000000u : 250000000u));
    analyzer.set_tracking_enabled(false);
    analyzer.set_cic_rate(68);
    analyzer.set_channel(0); // X
    analyzer.set_fft_navg(25);
    assert(analyzer.set_phase_precision(8));
    for (unsigned channel=0; channel<4; ++channel)
        analyzer.set_local_oscillator(channel, 10e6 + .637 * channel);

    // Build a real partially filled average through the production acquisition
    // thread, then stop the producer so publication is stable for the checks.
    hw::injected_x_status.store(8);
    hw::injected_y_status.store(8);
    auto producer=std::make_unique<SimulatedDma>(std::chrono::microseconds(5000));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (std::get<0>(analyzer.get_average_status()) < 3) {
        assert(std::chrono::steady_clock::now()<deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    producer.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    assert(std::get<3>(analyzer.get_precision_status())==1); // Valid

    const auto epoch=hw::simulated_epoch.load();
    const auto snapshot=analyzer.get_spectrum_snapshot();
    // Take count/target from this same atomic publication, rather than a
    // separate read that could precede the last queued capture's publication.
    const auto average=std::tuple{std::get<7>(snapshot), std::get<8>(snapshot)};
    assert(std::get<0>(average)>0 && std::get<1>(average)==25);
    const auto sequence=std::get<0>(snapshot);
    analyzer.set_cic_rate(68);
    analyzer.set_channel(0);
    analyzer.set_fft_navg(25);
    assert(analyzer.set_phase_precision(8));
    for (unsigned channel=0; channel<4; ++channel) {
        const auto frequency=rt::get_driver<Dds>().get_dds_freq(channel);
        analyzer.set_local_oscillator(channel, frequency);
        // Also cover a distinct double mapping to the same 48-bit word.
        analyzer.set_local_oscillator(channel, frequency + 1e-8);
    }
    assert(hw::simulated_epoch.load()==epoch);
    assert(std::get<0>(analyzer.get_spectrum_snapshot())==sequence);
    assert(analyzer.get_average_status()==average);

    for (unsigned channel : {1u, 2u}) { // Y and XY
        analyzer.set_channel(channel);
        const auto selected=std::get<0>(analyzer.get_spectrum_snapshot());
        analyzer.set_channel(channel);
        assert(std::get<0>(analyzer.get_spectrum_snapshot())==selected);
        assert(hw::simulated_epoch.load()==epoch);
    }

    // Reapplying the nominal LO after a tracking correction must retune and
    // restart, even though the nominal request itself has not changed.
    rt::get_driver<Dds>().set_dds_freq(2, 10e6 + 5, false);
    analyzer.set_local_oscillator(2, 10e6 + 2*.637);
    assert(hw::simulated_epoch.load()>epoch);
    assert(std::abs(rt::get_driver<Dds>().get_dds_freq(2) - (10e6+2*.637)) < 1e-6);

    // Switching preserves all four nominal/applied LOs and starts a new
    // paired DMA/calibration epoch. Repeated and invalid requests are no-ops.
    for (uint32_t rate : {200000000u, 250000000u, 200000000u, 250000000u}) {
        std::array<double, 4> applied;
        for (unsigned i = 0; i < 4; ++i) applied[i] = rt::get_driver<Dds>().get_dds_freq(i);
        const auto previous_rate = analyzer.get_sampling_frequency();
        const auto before = hw::simulated_epoch.load();
        assert(analyzer.set_sampling_frequency(rate));
        assert(analyzer.get_sampling_frequency() == rate && analyzer.get_cic_rate() == 68);
        assert(std::abs(std::get<1>(analyzer.get_parameters()).eval() - rate / 136.) < 1e-6);
        assert((hw::simulated_epoch.load() > before) == (rate != previous_rate));
        for (unsigned i = 0; i < 4; ++i)
            assert(std::abs(rt::get_driver<Dds>().get_dds_freq(i) - applied[i]) < 1e-6);
        const auto publication = std::get<0>(analyzer.get_spectrum_snapshot());
        assert(analyzer.set_sampling_frequency(rate));
        assert(!analyzer.set_sampling_frequency(240000000));
        assert(std::get<0>(analyzer.get_spectrum_snapshot()) == publication);
    }
    for (unsigned i = 0; i < 4; ++i) {
        analyzer.set_local_oscillator(i, 110e6);
        const auto before = hw::simulated_epoch.load();
        assert(!analyzer.set_sampling_frequency(200000000));
        assert(hw::simulated_epoch.load() == before && analyzer.get_sampling_frequency() == 250000000);
        analyzer.set_local_oscillator(i, 10e6 + .637 * i);
    }
    assert(analyzer.set_sampling_frequency(200000000));
    analyzer.save_config();
    assert(cfg.get<uint32_t>("PhaseNoiseAnalyzer", "sampling_frequency") == 200000000);

    const auto changed_epoch=hw::simulated_epoch.load();
    analyzer.set_cic_rate(20);
    assert(hw::simulated_epoch.load()>changed_epoch);
    assert(!analyzer.set_phase_precision(9));

    // Saturate the production consumer deliberately. Recovery must preserve
    // both rolling and cumulative averages, without restarting valid DMA.
    for (unsigned selected : {0u, 2u}) {
        analyzer.set_channel(selected);
        analyzer.set_cic_rate(selected == 0 ? 30 : 32);
        auto fast=std::make_unique<SimulatedDma>(std::chrono::microseconds(100));
        const auto filled_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while (std::get<0>(analyzer.get_average_status()) < 3) {
            assert(std::chrono::steady_clock::now()<filled_deadline);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        const auto stable_epoch=hw::simulated_epoch.load();
        const auto errors=std::get<6>(analyzer.get_precision_status());
        const auto initial_overruns=std::get<1>(analyzer.get_stream_status());
        auto count=std::get<0>(analyzer.get_average_status());
        while (std::get<1>(analyzer.get_stream_status()) < initial_overruns+3) {
            assert(std::chrono::steady_clock::now()<filled_deadline);
            const auto current=std::get<0>(analyzer.get_average_status());
            assert(current>=count);
            count=current;
            assert(hw::simulated_epoch.load()==stable_epoch);
            assert(std::get<6>(analyzer.get_precision_status())==errors);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        const auto [coverage_epoch, covered, span] = analyzer.get_stream_coverage();
        assert(covered > 0 && covered < span);
        analyzer.set_cic_rate(selected == 0 ? 32 : 34);
        const auto [reset_epoch, reset_covered, reset_span] = analyzer.get_stream_coverage();
        assert(reset_epoch > coverage_epoch && reset_covered == 0 && reset_span == 0);
        fast.reset();
        // Configure the next producer before the 100-ms watchdog expires.
    }
    std::cout << "Production four-channel settings passed: unchanged settings preserve averages/epoch/publication; corrected LO and changed rate restart; rolling/cumulative averages survive consumer overruns\n";
}
