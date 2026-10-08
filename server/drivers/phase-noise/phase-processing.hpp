#ifndef KOHERON_PHASE_PROCESSING_HPP
#define KOHERON_PHASE_PROCESSING_HPP

#include <array>
#include <cstdint>
#include <cassert>
#include <span>
#include <cmath>
#include <scicpp/core.hpp>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

template<typename Phase, std::size_t N>
void convert_relative_phase(const std::array<int32_t, N>& raw,
                            std::array<Phase, N>& phase, Phase radians_per_count) {
    static_assert(N > 0);
    const int32_t origin = raw.front();
    for (std::size_t i = 0; i < N; ++i) {
        // The full difference fits an unsigned 32-bit magnitude. Unsigned
        // subtraction is defined across the signed endpoints and avoids
        // the ARM software helper for int64_t-to-float conversion.
        const float difference = raw[i] >= origin ?
            float(uint32_t(raw[i]) - uint32_t(origin)) :
            -float(uint32_t(origin) - uint32_t(raw[i]));
        phase[i] = radians_per_count * difference;
    }
}

template<typename Phase, std::size_t N>
std::array<Phase, N> relative_phase_snapshot(const std::array<int32_t, N>& raw,
                                            Phase radians_per_count) {
    std::array<Phase, N> result;
    convert_relative_phase(raw, result, radians_per_count);
    return result;
}

namespace phase_noise {

// Keep the fit in raw counts. Casting a large carrier-drift ramp to float
// before removing it can discard phase increments that the FPGA retained.
struct RawPhaseTrend {
    double anchor;
    double mean;
    double slope;
};

// Keep the integer reduction out of the large preparation/FFT caller. GCC's
// ARM LTO inlining otherwise spills NEON accumulators inside the sample loop.
[[gnu::noinline]] inline RawPhaseTrend fit_raw_phase(std::span<const int32_t> raw) {
    const auto samples = raw.size();
    assert(samples > 1 && samples <= 65536);
    int64_t sum = 0, twice_covariance = 0;
    std::size_t i = 0;
#if defined(__ARM_NEON)
    const int32_t first = -int32_t(samples - 1);
    const int32_t weights[]{first, first + 2, first + 4, first + 6};
    auto weight = vld1q_s32(weights);
    auto totals = vdupq_n_s64(0), covariance = vdupq_n_s64(0);
    const auto vector_samples = samples - samples % 4;
    for (; i < vector_samples; i += 4) {
        const auto value = vld1q_s32(raw.data() + i);
        totals = vaddq_s64(totals, vaddq_s64(vmovl_s32(vget_low_s32(value)), vmovl_s32(vget_high_s32(value))));
        covariance = vaddq_s64(covariance, vmull_s32(vget_low_s32(value), vget_low_s32(weight)));
        covariance = vaddq_s64(covariance, vmull_s32(vget_high_s32(value), vget_high_s32(weight)));
        weight = vaddq_s32(weight, vdupq_n_s32(8));
    }
    sum = vgetq_lane_s64(totals, 0) + vgetq_lane_s64(totals, 1);
    twice_covariance = vgetq_lane_s64(covariance, 0) + vgetq_lane_s64(covariance, 1);
#endif
    for (; i < samples; ++i) {
        const auto weight = int32_t(2 * i) - int32_t(samples - 1);
        sum += int64_t(raw[i]);
        twice_covariance += int64_t(weight) * int64_t(raw[i]);
    }
    const double n = double(samples), anchor = double(raw[samples / 2]);
    return {anchor, double(sum) / n - anchor,
            double(twice_covariance) / (n * (n * n - 1.0) / 6.0)};
}

inline void prepare_phase_window_reference(std::span<const int32_t> raw, const RawPhaseTrend& trend,
                                           double scale, std::span<const float> window, float* output) {
    assert(raw.size() > 1 && raw.size() <= 65536 && raw.size() == window.size());
    const double mean = trend.anchor + trend.mean;
    const double center = double(raw.size() - 1) / 2;
#pragma GCC unroll 4
    for (std::size_t i = 0; i < raw.size(); ++i)
        output[i] = float((double(raw[i]) - mean - trend.slope * (double(i) - center)) * scale) * window[i];
}

inline void prepare_phase_window(std::span<const int32_t> raw, const RawPhaseTrend& trend,
                                 double scale, std::span<const float> window, float* output) {
    assert(raw.size() > 1 && raw.size() <= 65536 && raw.size() == window.size());
#if defined(__ARM_NEON)
    const double mean = trend.anchor + trend.mean;
    const double center = double(raw.size() - 1) / 2;
    // Subtract the fitted ramp in signed Q31.32 BEFORE converting to float.
    // Out-of-range fitted lines/residuals use the double fallback. Rounding the
    // intercept/slope adds at most (N + 1)/2^33 raw counts (< 7.7e-6 at N=65536).
    // This retains small steps on a full-range ramp without per-sample double
    // arithmetic or ARM's software int64-to-float conversion helpers.
    const double first_line = mean - trend.slope * center;
    const double last_line = mean + trend.slope * center;
    if (std::abs(scale) >= 1e-12 && std::abs(scale) <= 1e12 &&
        std::abs(first_line) < 2147483647.5 && std::abs(last_line) < 2147483647.5) {
        constexpr double unit = 4294967296.0;
        const int64_t first = std::llround(first_line * unit);
        const int64_t step = std::llround(trend.slope * unit);
        const int64_t initial[]{first, first + step};
        auto line = vld1q_s64(initial);
        const auto advance = vdupq_n_s64(2 * step);
        const float factor = float(scale);
        auto errors = vdupq_n_s64(0);
        std::size_t i = 0;
        for (; i + 4 <= raw.size(); i += 4) {
            const auto input = vld1q_s32(raw.data() + i);
            const auto low_raw = vshlq_n_s64(vmovl_s32(vget_low_s32(input)), 32);
            const auto low = vsubq_s64(low_raw, line);
            errors = vorrq_s64(errors, vandq_s64(veorq_s64(low_raw, line), veorq_s64(low_raw, low)));
            line = vaddq_s64(line, advance);
            const auto high_raw = vshlq_n_s64(vmovl_s32(vget_high_s32(input)), 32);
            const auto high = vsubq_s64(high_raw, line);
            errors = vorrq_s64(errors, vandq_s64(veorq_s64(high_raw, line), veorq_s64(high_raw, high)));
            line = vaddq_s64(line, advance);
            const auto low_sign = vshrq_n_s64(low, 63), high_sign = vshrq_n_s64(high, 63);
            const auto low_abs = vreinterpretq_u64_s64(vsubq_s64(veorq_s64(low, low_sign), low_sign));
            const auto high_abs = vreinterpretq_u64_s64(vsubq_s64(veorq_s64(high, high_sign), high_sign));
            // Convert the magnitude to avoid cancellation for negative
            // sub-count residuals, then restore its sign through the float bits.
            const auto integers = vcombine_u32(vmovn_u64(vshrq_n_u64(low_abs, 32)), vmovn_u64(vshrq_n_u64(high_abs, 32)));
            const auto fractions = vcombine_u32(vmovn_u64(low_abs), vmovn_u64(high_abs));
            const auto magnitude = vaddq_f32(vcvtq_f32_u32(integers),
                                            vmulq_n_f32(vcvtq_f32_u32(fractions), 0x1p-32f));
            const auto sign = vreinterpretq_u32_s32(vcombine_s32(vmovn_s64(low_sign), vmovn_s64(high_sign)));
            const auto residual = vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(magnitude),
                vandq_u32(sign, vdupq_n_u32(0x80000000u))));
            vst1q_f32(output + i, vmulq_f32(vmulq_n_f32(residual, factor), vld1q_f32(window.data() + i)));
        }
        // Reduce overflow flags once per window, keeping the sample loop in
        // NEON. No result escapes before an exceptional window is recomputed.
        const auto signs = vshrq_n_s64(errors, 63);
        const bool overflow = (vgetq_lane_s64(signs, 0) | vgetq_lane_s64(signs, 1)) != 0;
        for (; !overflow && i < raw.size(); ++i)
            output[i] = float((double(raw[i]) - mean - trend.slope * (double(i) - center)) * scale) * window[i];
        if (!overflow) return;
    }
#endif
    prepare_phase_window_reference(raw, trend, scale, window, output);
}

template<std::size_t Samples, std::size_t N>
RawPhaseTrend fit_raw_phase_prefix(const std::array<int32_t, N>& raw) {
    // Centered weights sum to zero. At up to 65536 samples, both exact
    // integer sums fit int64_t even for the full signed 32-bit input range.
    static_assert(Samples > 1 && Samples <= N && Samples <= 65536);
    return fit_raw_phase(std::span<const int32_t>(raw.data(), Samples));
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
