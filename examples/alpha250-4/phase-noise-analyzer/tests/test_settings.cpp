// Exercise production settings against the descriptor-ring hardware stub.
#include "../phase-noise-analyzer.hpp"
#include "simulated_dma.hpp"
#include <cassert>
#include <iostream>
#include <memory>

int main() {
    PhaseNoiseAnalyzer analyzer;
    analyzer.set_tracking_enabled(false);
    analyzer.set_cic_rate(67);
    analyzer.set_channel(0); // X
    analyzer.set_fft_navg(25);
    assert(analyzer.set_phase_precision(8));
    for (unsigned channel=0; channel<4; ++channel)
        analyzer.set_local_oscillator(channel, 10e6 + .637 * channel);

    // Build a real partially filled average through the production acquisition
    // thread, then stop the producer so publication is stable for the checks.
    hw::injected_x_status.store(8);
    hw::injected_y_status.store(8);
    auto producer=std::make_unique<SimulatedDma>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (std::get<0>(analyzer.get_average_status()) < 3) {
        assert(std::chrono::steady_clock::now()<deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    producer.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(std::get<3>(analyzer.get_precision_status())==1); // Valid

    const auto epoch=hw::simulated_epoch.load();
    const auto snapshot=analyzer.get_spectrum_snapshot();
    // Take count/target from this same atomic publication, rather than a
    // separate read that could precede the last queued capture's publication.
    const auto average=std::tuple{std::get<7>(snapshot), std::get<8>(snapshot)};
    assert(std::get<0>(average)>0 && std::get<1>(average)==25);
    const auto sequence=std::get<0>(snapshot);
    analyzer.set_cic_rate(67);
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

    const auto changed_epoch=hw::simulated_epoch.load();
    analyzer.set_cic_rate(20);
    assert(hw::simulated_epoch.load()>changed_epoch);
    assert(!analyzer.set_phase_precision(9));
    std::cout << "Production four-channel settings passed: unchanged settings preserve averages/epoch/publication; corrected LO and changed rate restart\n";
}
