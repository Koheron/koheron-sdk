#pragma once
#include "server/hardware/memory_manager.hpp"
#include "server/runtime/driver_manager.hpp"
#include <cstdint>

namespace dpll {
// Instrument policies select a clock driver and the DAC route encoding.
template<class Clock, uint32_t SelectBits = 3, uint32_t Sources = 8>
struct ClockedBoard {
    using ClockDriver = Clock;
    static constexpr uint32_t dac_select_bits = SelectBits;
    static constexpr uint32_t dac_sources = Sources;
    static_assert(SelectBits > 0 && 2 * SelectBits <= 32);
    static_assert(Sources > 0 && Sources <= (uint64_t{1} << SelectBits));
    void initialize() {
        static_assert(prm::adc_clk == 200000000 || prm::adc_clk == 250000000);
        rt::get_driver<Clock>().set_sampling_frequency(prm::adc_clk == 250000000 ? 1 : 0);
    }
    double sampling_frequency() const {
        return rt::get_driver<Clock>().get_dac_sampling_freq();
    }
};
}
