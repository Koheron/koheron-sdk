// Exercise the production PNA core's passive policy with controlled DMA epochs.
#include "server/drivers/phase-noise/core.hpp"
#include <cassert>
#include <iostream>

struct References {
    inline static std::atomic<unsigned> writes{0};
    double get_dds_freq(uint32_t channel) { return channel ? 12e6 : 10e6; }
    void set_dds_freq(uint32_t, double) { ++writes; }
};
struct MonitorBoard {
    using Oscillator = References;
    static constexpr bool passive_monitor = true;
    static constexpr uint32_t max_phase_precision = 8;
    static constexpr uint32_t cic_rate_step = 2;
    inline static std::atomic<uint64_t> revision{0};
    inline static unsigned initialized = 0;
    void initialize_phase() { ++initialized; }
    void select_channel(uint32_t channel) {
        hw::get_memory<mem::control>().write_mask<reg::cordic, 16>(channel << 4);
    }
    uint32_t demodulated(uint32_t) { return 0; }
    void set_phase_precision(uint32_t bits) {
        hw::get_memory<mem::control>().write<reg::phase_precision>(bits);
    }
    uint32_t phase_packet_status() { return 0; }
    double sampling_frequency() { return 250e6; }
    uint32_t reference_clock() { return 2; }
    uint64_t configuration_revision() { return revision; }
    double power_conversion(uint32_t, double) { return 1; }
};

int main() {
    auto& engine = rt::get_driver<DmaS2MM>();
    auto& ctl = hw::get_memory<mem::control>();
    // Existing loop accumulator mask must survive construction, channel/rate/
    // precision changes, errors and a loop-edit-induced monitor restart.
    ctl.words[reg::cordic / 4] = 2;
    {
        phase_noise::Core<MonitorBoard> monitor;
        unsigned waits = 1;
        engine.wait_until(waits);
        auto capture = [&](bool success = true) {
            engine.complete(success);
            engine.wait_until(++waits);
        };
        auto state = [&] { return std::get<3>(monitor.get_precision_status()); };
        auto valid = [&] {
            for (unsigned i = 0; i < 8 && state() != 1; ++i) capture();
            assert(state() == 1);
        };
        assert(MonitorBoard::initialized == 1);
        assert(References::writes == 0);
        assert((ctl.words[reg::cordic / 4] & 3) == 2);
        assert(std::get<1>(monitor.get_parameters()).eval() == 6250000.f);
        monitor.set_cic_rate(21);
        assert(std::get<3>(monitor.get_parameters()) == 20);
        valid();
        auto old = monitor.get_spectrum_snapshot();
        assert(std::get<1>(old) == 1 && std::get<9>(old) == 10e6);
        monitor.set_fft_navg(4);
        capture(); capture();
        assert(std::get<0>(monitor.get_average_status()) > 1);
        // A loop edit during an outstanding capture must discard it and clear
        // the old averaging epoch; settings writes never wait for that capture.
        ++MonitorBoard::revision;
        capture();
        assert(state() == 0);
        assert(std::get<0>(monitor.get_average_status()) == 0);
        valid();
        monitor.set_channel(1);
        monitor.set_cic_rate(32);
        assert(monitor.set_phase_precision(8));
        capture(); valid();
        const auto settings = monitor.get_parameters();
        assert(std::get<2>(settings) == 1 && std::get<3>(settings) == 32);
        assert(std::get<1>(settings).eval() == 3906250.f);
        assert(std::get<1>(monitor.get_precision_status()) == 8);
        hw::injected_packet_flags = 0x40;
        capture();
        assert(state() == 4);
        assert(std::get<0>(monitor.get_average_status()) == 0);
        hw::injected_packet_flags = 0;
        valid();
        monitor.reset_average();
        assert(state() == 0 && std::get<0>(monitor.get_average_status()) == 0);
        assert(!monitor.set_phase_precision(9));
        assert(References::writes == 0);
        assert((ctl.words[reg::cordic / 4] & 3) == 2);
        engine.cancel();
    }
    std::cout << "PASS: passive PNA monitor epochs, calibration, gaps and feedback ownership\n";
}
