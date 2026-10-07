#pragma once
#include <cstdint>
#include <mutex>
namespace clock_cfg { inline std::recursive_mutex sampling_mutex; }
namespace hw { inline double test_sampling_frequency = 200e6; }
class ClockGenerator {
public:
    bool switchable = false;
    void use_ps_phase_control(uint32_t) {}
    void set_sampling_frequency(uint32_t selection) {
        if (switchable) hw::test_sampling_frequency = selection == 0 ? 200e6 : 250e6;
    }
    double get_adc_sampling_freq() { return hw::test_sampling_frequency; }
    double get_dac_sampling_freq() { return 250e6; } // deliberately different
    uint32_t get_reference_clock() { return 0; }
};
