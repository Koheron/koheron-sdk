#ifndef ALPHA250_4_PHASE_PROCESSING_HPP
#define ALPHA250_4_PHASE_PROCESSING_HPP

#include <array>
#include <cstdint>
#include <scicpp/core.hpp>

template<typename Phase, std::size_t N>
void convert_relative_phase(const std::array<int32_t, N>& raw,
                            std::array<Phase, N>& phase, Phase radians_per_count) {
    static_assert(N > 0);
    uint32_t previous = static_cast<uint32_t>(raw.front());
    int64_t relative = 0;
    for (std::size_t i = 0; i < N; ++i) {
        // DMA contains the low 32 bits of the wider fixed-point phase. Choose
        // adjacent signed displacements modulo 2^32, then accumulate in 64
        // bits so a snapshot may cross the phase-counter boundary repeatedly.
        // Each sample step must be less than half the phase-counter range.
        const uint32_t current = static_cast<uint32_t>(raw[i]);
        const uint32_t delta = current - previous;
        const int64_t displacement = delta <= uint32_t{INT32_MAX}
            ? int64_t(delta) : int64_t(delta) - (int64_t{1} << 32);
        relative += displacement;
        phase[i] = radians_per_count * float(relative);
        previous = current;
    }
}

template<std::size_t Samples, typename Phase, std::size_t N>
Phase phase_slope_per_sample(const std::array<Phase, N>& phase) {
    static_assert(Samples > 1 && Samples <= N);
    // Fit the entire block rather than letting two noisy endpoints determine
    // the frequency estimate. Center both axes to preserve small increments.
    const double center = double(Samples - 1) / 2.0;
    const double anchor = phase[Samples / 2].eval();
    double covariance = 0.0;
    for (std::size_t i = 0; i < Samples; ++i)
        covariance += (double(i) - center) * (double(phase[i].eval()) - anchor);
    const double n = double(Samples);
    return Phase{float(covariance / (n * (n * n - 1.0) / 12.0))};
}

template<std::size_t Samples, typename Phase, std::size_t N>
std::array<Phase, Samples> detrended_phase_prefix(const std::array<Phase, N>& phase) {
    static_assert(Samples > 1 && Samples <= N);
    // Remove the carrier's constant phase and frequency offset before applying
    // spectral windows. Keep the original phase snapshot for slope telemetry.
    const double center = double(Samples - 1) / 2.0;
    const double anchor = phase[Samples / 2].eval();
    double sum = 0.0, covariance = 0.0;
    for (std::size_t i = 0; i < Samples; ++i) {
        const double value = double(phase[i].eval()) - anchor;
        sum += value;
        covariance += (double(i) - center) * value;
    }
    const double n = double(Samples);
    const double slope = covariance / (n * (n * n - 1.0) / 12.0);
    const double mean = sum / n;
    std::array<Phase, Samples> result{};
    for (std::size_t i = 0; i < Samples; ++i)
        result[i] = Phase{float((double(phase[i].eval()) - anchor) - mean - slope * (double(i) - center))};
    return result;
}

#endif
