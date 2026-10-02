#pragma once
#include <array>
#include <cstdint>
class Ltc2157 {
public:
    double get_input_voltage_range(uint32_t) { return 1.0; }
    template<class T> auto tf_polynomial(uint32_t) { return std::array<T, 1>{T{1}}; }
};
