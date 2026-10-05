#pragma once
#include <array>
class Ltc2157 {
  public:
    float get_input_voltage_range(unsigned, unsigned) { return 1.0f; }
    template<typename T> auto tf_polynomial(unsigned, unsigned) {
        return std::array<T, 6>{1, 0, 0, 0, 0, 0};
    }
};
