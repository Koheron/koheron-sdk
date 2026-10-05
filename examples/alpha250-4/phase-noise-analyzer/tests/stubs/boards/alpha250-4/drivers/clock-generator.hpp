#pragma once
#include <array>
#include <cstdint>
class ClockGenerator {
  public:
    void set_sampling_frequency(unsigned) {}
    auto get_adc_sampling_freq() { return std::array<double, 2>{200e6, 200e6}; }
    uint32_t get_reference_clock() { return 0; }
};
