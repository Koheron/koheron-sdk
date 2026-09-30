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
