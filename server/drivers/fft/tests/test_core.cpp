#include "server/drivers/fft/core.hpp"
#include <cassert>
#include <iostream>
#include <type_traits>

struct Calibration {
    template <unsigned Channel, unsigned N> auto get_inverse_transfer_function(double) {
        std::array<float, N> values;
        values.fill(1);
        return values;
    }
    double get_input_voltage_range(uint32_t) { return 2; }
};
struct Board {
    inline static std::atomic<double> rate{prm::adc_clk};
    double sampling_frequency() { return rate; }
    Calibration& calibration() { static Calibration value; return value; }
};
using Core = fft::Core<Board>;
using Spectrum = std::array<float, prm::fft_size/2>;
static_assert(std::is_same_v<decltype(std::declval<Core&>().read_psd()), Spectrum>);
static_assert(std::is_same_v<decltype(std::declval<Core&>().read_psd_raw()), Spectrum>);

template <typename Predicate> void eventually(Predicate predicate) {
    for (int i = 0; i < 200; ++i) {
        if (predicate()) { return; }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    assert(false && "Acquisition did not advance");
}
int main() {
    {
        Core core;
        assert(core.get_control_state().window == 1);
        fake::registers[reg::adc0] = (1U << prm::adc_width) - 1;
        fake::registers[reg::adc1] = 1U << (prm::adc_width - 1);
        assert((core.get_adc_raw_data(4) == std::array<int32_t, 2>{-1, -int32_t(1U << (prm::adc_width - 1))}));
        core.set_input_channel(1);
        core.set_input_channel(2);
        assert(core.get_control_state().channel == 1);
        core.set_fft_window(0);
        auto state = core.get_control_state();
        assert(state.w1 == 1 && state.w2 == 1);
        for (auto word : fake::window) { assert(word == 32768); }
        core.set_fft_window(99);
        assert(core.get_control_state().window == 0);
        eventually([&] { return core.read_psd_raw()[0] == 3; });
        auto snapshot = core.read_psd_raw();
        fake::spectrum = 7;
        eventually([&] { return core.read_psd_raw()[0] == 7; });
        assert(snapshot[0] == 3);
        const double expected = 7 * std::pow(2.0 / (2 << 20), 2) / prm::n_cycles / prm::adc_clk / 50;
        assert(std::abs(core.read_psd()[0] / expected - 1) < 1e-6);
        core.set_dds_freq(0, prm::adc_clk);
        assert(fake::registers[reg::phase_incr0] == (1U << 31));
        core.set_dds_freq(0, 1e6);
        Board::rate = prm::adc_clk / 2;
        eventually([&] { return core.get_control_state().sample_rate == prm::adc_clk / 2; });
        assert(fake::registers[reg::phase_incr0] == uint32_t((uint64_t{1} << 32) * 1e6 / (prm::adc_clk / 2)));
        for (uint32_t window = 1; window < 4; ++window) {
            core.set_fft_window(window);
            state = core.get_control_state();
            assert(state.window == window && state.w1 > 0 && state.w2 > 0);
        }
        // Destruction must join even when hardware stops advancing.
        fake::advance = false;
    }
    std::cout << "PASS: " << prm::fft_size << " points, " << prm::adc_width
              << " bits: snapshots, normalization, ADC sign, controls, clock changes, shutdown\n";
}
