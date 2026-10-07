#pragma once
#include <array>
#include <cstdint>
#include <mutex>
namespace clock_cfg { inline std::recursive_mutex sampling_mutex; }
class ClockGenerator {
  public:
    bool switchable = false;
    double rate = 200e6;
    void use_ps_phase_control(uint32_t) {}
    void set_sampling_frequency(unsigned selection) {
        std::lock_guard lock(clock_cfg::sampling_mutex);
        if (switchable) rate = selection == 0 ? 200e6 : 250e6;
    }
    auto get_adc_sampling_freq() {
        std::lock_guard lock(clock_cfg::sampling_mutex);
        return std::array<double, 2>{rate, rate};
    }
    uint32_t get_reference_clock() { return 0; }
};
