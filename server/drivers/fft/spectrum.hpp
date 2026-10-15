#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace fft {

template <typename Factors>
bool valid_power_conversion(double voltage_range, const Factors& factors) {
    return std::isfinite(voltage_range) && std::abs(voltage_range) > 0 &&
        std::all_of(factors.begin(), factors.end(), [](float value) {
            return std::isfinite(value) && value > 0;
        });
}

// The shared datapath aligns ADC samples to signed 16-bit codes and multiplies
// its Q15 window through a fixed-to-float conversion with 16 fractional bits.
// Its full-scale sine therefore has amplitude 2^14 at the FFT input. Normalize
// the one-sided density to that sine's mean-square power (0 dBFS), retaining
// the actual FFT length, accumulation count, sample rate and window energy.
template <std::size_t Bins>
auto full_scale_density(std::array<float, Bins> raw, uint32_t averages,
                       double sample_rate, double window_energy) {
    const double factor = std::ldexp(1.0, -26) /
        (2 * Bins) / averages / sample_rate / window_energy;
    for (auto& value : raw) { value = float(double(value) * factor); }
    raw[0] *= .5f; // DC has no negative-frequency partner.
    return raw;
}

} // namespace fft
