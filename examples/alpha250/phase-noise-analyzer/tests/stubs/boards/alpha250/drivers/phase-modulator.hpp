#pragma once
#include "boards/alpha250/drivers/clock-generator.hpp"
#include "server/runtime/driver_manager.hpp"
class PhaseModulator {
public:
    bool compatible = true, fail_change = false;
    bool sample_rate_compatible(uint32_t rate) { return compatible && (rate == 200000000 || rate == 250000000); }
    bool change_sample_rate(uint32_t rate) {
        if (fail_change) return false;
        rt::get_driver<ClockGenerator>().set_sampling_frequency(rate == 200000000 ? 0 : 1);
        return true;
    }
};
