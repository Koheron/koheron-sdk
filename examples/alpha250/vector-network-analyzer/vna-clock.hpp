#pragma once
#include "boards/alpha250/drivers/clock-generator.hpp"
#include <cmath>
#include <mutex>
struct VNAClock {
    static constexpr double rate = 250000000.0;
    static std::recursive_mutex& mutex() { return clock_cfg::sampling_mutex; }
    static bool matches() {
        auto& clock = rt::get_driver<ClockGenerator>();
        return std::abs(clock.get_adc_sampling_freq() - rate) < .5 &&
            clock.get_reference_clock() == clock_cfg::TCXO_CLOCK;
    }
    static void ensure() {
        auto& clock = rt::get_driver<ClockGenerator>();
        if (clock.get_reference_clock() != clock_cfg::TCXO_CLOCK)
            clock.set_reference_clock(clock_cfg::TCXO_CLOCK);
        if (std::abs(clock.get_adc_sampling_freq() - rate) >= .5) clock.set_sampling_frequency(1);
    }
};
