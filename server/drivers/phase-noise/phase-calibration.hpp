#ifndef __PHASE_NOISE_PHASE_CALIBRATION_HPP__
#define __PHASE_NOISE_PHASE_CALIBRATION_HPP__

#include <cstdint>

namespace phase_calibration {

// Inverse DC gain for this instrument's 32-bit CIC/FIR phase path.
// The FIR's 32 fractional coefficient bits and 66 -> 32-bit output give
// a gain of 1/4. CIC truncation divides (R*M)^N by the next power of two.
// Call only with a valid positive rate, delay and stage count.
constexpr double filter_correction(uint32_t rate, uint32_t stages, uint32_t delay) {
    double cic_gain = 1.0;
    for (uint32_t i = 0; i < stages; ++i) {
        cic_gain *= double(rate) * double(delay);
    }

    // Normalize without integer overflow or log2 rounding at powers of two.
    while (cic_gain > 1.0) {
        cic_gain *= 0.5;
    }
    return 4.0 / cic_gain;
}

} // namespace phase_calibration

#endif // __PHASE_NOISE_PHASE_CALIBRATION_HPP__
