#pragma once
#include <array>
class ClockGenerator {
  public:
    std::array<double, 2> frequencies{200000000.0, 200000000.0};
    auto get_adc_sampling_freq() const { return frequencies; }
};
