#ifndef ALPHA250_4_PHASE_SCALING_HPP
#define ALPHA250_4_PHASE_SCALING_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>

struct PhaseScaling {
    uint32_t dut;
    uint32_t reference;
    double output_scale;
};

inline double cic_gain_compensation(uint32_t rate, uint32_t stages, uint32_t delay) {
    // PG140: programmable truncated output shifts by the current rate's bit growth.
    // Input and output are both 32 bits, so DC gain is (R*M)^N / 2^ceil(log2((R*M)^N)).
    const double gain = std::pow(double(rate) * delay, stages);
    int exponent = 0;
    const double fraction = std::frexp(gain, &exponent);
    return std::ldexp(1.0, fraction <= 0.5 ? exponent - 1 : exponent) / gain;
}

inline PhaseScaling phase_scaling(double dut_hz, double reference_hz) {
    constexpr double unity = uint32_t{1} << 30;
    // A disabled oscillator has no meaningful frequency ratio.
    const double ratio = dut_hz > 0.0 && reference_hz > 0.0
        ? dut_hz / reference_hz : 1.0;
    const double scale = std::max(1.0, ratio);
    // Q2.30 coefficients stay in [0, 1], avoiding multiplier overflow.
    return {static_cast<uint32_t>(std::llround(unity / scale)),
            static_cast<uint32_t>(std::llround(unity * ratio / scale)), scale};
}

#endif
