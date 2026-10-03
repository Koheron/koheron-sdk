#ifndef KOHERON_PHASE_PROCESSING_HPP
#define KOHERON_PHASE_PROCESSING_HPP

#include <array>
#include <cstdint>
#include <scicpp/core.hpp>

template<typename Phase, std::size_t N>
void convert_relative_phase(const std::array<int32_t, N>& raw,
                            std::array<Phase, N>& phase, Phase radians_per_count) {
    static_assert(N > 0);
    const int64_t origin = raw.front();
    for (std::size_t i = 0; i < N; ++i)
        phase[i] = radians_per_count * float(int64_t(raw[i]) - origin);
}

namespace phase_noise {

// Keep the fit in raw counts. Casting a large carrier-drift ramp to float
// before removing it can discard phase increments that the FPGA retained.
struct RawPhaseTrend {
    double anchor;
    double mean;
    double slope;
};

template<std::size_t Samples, std::size_t N>
RawPhaseTrend fit_raw_phase_prefix(const std::array<int32_t, N>& raw) {
    static_assert(Samples > 1 && Samples <= N);
    const double center = double(Samples - 1) / 2.0;
    const double anchor = double(raw[Samples / 2]);
    double sum = 0.0, covariance = 0.0;
    for (std::size_t i = 0; i < Samples; ++i) {
        const double value = double(raw[i]) - anchor;
        sum += value;
        covariance += (double(i) - center) * value;
    }
    const double n = double(Samples);
    return {anchor, sum / n, covariance / (n * (n * n - 1.0) / 12.0)};
}

template<std::size_t Samples, typename Phase, std::size_t N>
std::array<Phase, Samples> detrended_raw_phase_prefix(
    const std::array<int32_t, N>& raw, const RawPhaseTrend& trend, Phase radians_per_count) {
    static_assert(Samples > 1 && Samples <= N);
    const double center = double(Samples - 1) / 2.0;
    const double scale = double(radians_per_count.eval());
    std::array<Phase, Samples> result{};
    for (std::size_t i = 0; i < Samples; ++i)
        result[i] = Phase{float(((double(raw[i]) - trend.anchor) - trend.mean -
                                 trend.slope * (double(i) - center)) * scale)};
    return result;
}

} // namespace phase_noise

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
