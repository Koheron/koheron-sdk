#pragma once
#include <cstdint>
class ClockGenerator {
public:
    void set_sampling_frequency(uint32_t) {}
    double get_adc_sampling_freq() { return 200e6; }
    double get_dac_sampling_freq() { return 250e6; } // deliberately different
    uint32_t get_reference_clock() { return 0; }
};
