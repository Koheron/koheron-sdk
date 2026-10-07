#pragma once

#include <scicpp/core.hpp>
#include <scicpp/signal/windows.hpp>
#include "server/external_libs/pffft/pffft.h"
#include "phase-processing.hpp"
#include "fft-layout.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <future>
#include <memory>
#include <type_traits>
#include <vector>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace phase_noise {

namespace detail {
// Compact native-order power into N/2+1 floats, without storing zero imaginary
// lanes. DC stays first and Nyquist stays last; SIMD ordinary bins retain the
// transform's four-lane block order until final density publication.
template<bool Accumulate = true>
inline void accumulate_welch_power(const float* transformed, float* power,
                                   std::size_t fft_size, int simd_size, float normalization = 1.f) {
    const auto store = [&](std::size_t bin, float value) {
        if constexpr (Accumulate) power[bin] += value;
        else power[bin] = value * (bin == 0 || bin == fft_size / 2 ? normalization : 2.f * normalization);
    };
    if (simd_size == 1) {
        store(0, transformed[0] * transformed[0]);
        store(fft_size / 2, transformed[fft_size - 1] * transformed[fft_size - 1]);
        for (std::size_t i = 1; i < fft_size - 1; i += 2)
            store((i + 1) / 2, transformed[i] * transformed[i] + transformed[i + 1] * transformed[i + 1]);
        return;
    }
    assert(simd_size == 4);
    // SIMD native layout is four real lanes followed by four imaginary lanes.
    // In the first block, lane 0 is DC and lane 4 is Nyquist, not a complex pair.
    store(0, transformed[0] * transformed[0]);
    store(fft_size / 2, transformed[4] * transformed[4]);
    for (std::size_t i = 1; i < 4; ++i)
        store(i, transformed[i] * transformed[i] + transformed[i + 4] * transformed[i + 4]);
    std::size_t i = 8;
#if defined(__ARM_NEON)
    const bool normal_factor = Accumulate ||
        (std::bit_cast<uint32_t>(2.f * normalization) & 0x7fffffffu) >= 0x00800000u;
    const float normalized_minimum = Accumulate ? 0.f : std::nextafter(
        std::max(std::numeric_limits<float>::min(),
                 float(double(std::numeric_limits<float>::min()) / (2.0 * double(normalization)))),
        std::numeric_limits<float>::infinity());
    for (; i < fft_size; i += 8) {
        const auto real = vld1q_f32(transformed + i);
        const auto imaginary = vld1q_f32(transformed + i + 4);
        const auto old = Accumulate ? vld1q_f32(power + i / 2) : vdupq_n_f32(0);
        const auto magnitude = [](float32x4_t v) {
            return vandq_u32(vreinterpretq_u32_f32(v), vdupq_n_u32(0x7fffffffu));
        };
        const auto safe = [](uint32x4_t bits, uint32_t minimum) {
            return vorrq_u32(vceqq_u32(bits, vdupq_n_u32(0)),
                vcgeq_u32(bits, vdupq_n_u32(minimum)));
        };
        // ARMv7 NEON flushes subnormals. Inspect the bits, since even a floating
        // comparison can flush its operand. Fall back when squaring or adding
        // could lose a nonzero subnormal that scalar VFP would retain.
        auto valid = vandq_u32(safe(magnitude(real), 0x20000000u),
                              safe(magnitude(imaginary), 0x20000000u)); // sqrt(FLT_MIN)
        valid = vandq_u32(valid, safe(magnitude(old), 0x00800000u)); // FLT_MIN
        const auto halves = vreinterpretq_u64_u32(valid);
        bool valid_lanes = (vgetq_lane_u64(halves, 0) & vgetq_lane_u64(halves, 1)) == UINT64_MAX;
        if constexpr (!Accumulate) {
            // The fused normalization must also preserve nonzero subnormals.
            // Raw products are checked above before using the NEON result.
            const auto norm = vaddq_f32(vmulq_f32(real, real), vmulq_f32(imaginary, imaginary));
            const auto bits = vreinterpretq_u32_f32(norm);
            const auto mask = vorrq_u32(vceqq_u32(bits, vdupq_n_u32(0)),
                vcgeq_u32(bits, vreinterpretq_u32_f32(vdupq_n_f32(normalized_minimum))));
            const auto lanes = vreinterpretq_u64_u32(mask);
            valid_lanes &= normal_factor && (vgetq_lane_u64(lanes, 0) & vgetq_lane_u64(lanes, 1)) == UINT64_MAX;
        }
        if (valid_lanes) {
            const auto norm = vaddq_f32(vmulq_f32(real, real), vmulq_f32(imaginary, imaginary));
            if constexpr (Accumulate) vst1q_f32(power + i / 2, vaddq_f32(old, norm));
            else vst1q_f32(power + i / 2, vmulq_n_f32(norm, 2.f * normalization));
        } else {
            for (std::size_t j = i; j < i + 4; ++j)
                store(i / 2 + j - i, transformed[j] * transformed[j] + transformed[j + 4] * transformed[j + 4]);
        }
    }
#endif
    for (; i < fft_size; i += 8)
        for (std::size_t j = i; j < i + 4; ++j)
            store(i / 2 + j - i, transformed[j] * transformed[j] + transformed[j + 4] * transformed[j + 4]);
}
} // namespace detail

// Acquisition owns this estimator. A read-only plan and two buffer sets survive
// across captures; one background task processes alternate Welch segments.
template<std::size_t FftSize>
class WelchSpectrum {
    static_assert(FftSize >= 32 && (FftSize & (FftSize - 1)) == 0);
    static_assert(FftSize <= (1u << 24), "Bin markers must be exact float integers");
    static constexpr std::size_t bins = FftSize / 2 + 1;
    std::vector<float> window = scicpp::signal::windows::hann<float>(FftSize);
    struct AlignedFree {
        void operator()(float* pointer) const { pffft_aligned_free(pointer); }
    };
    using Buffer = std::unique_ptr<float, AlignedFree>;
    static Buffer make_buffer() {
        Buffer result{static_cast<float*>(pffft_aligned_malloc(FftSize * sizeof(float)))};
        assert(result);
        return result;
    }
    std::unique_ptr<PFFFT_Setup, decltype(&pffft_destroy_setup)> setup{
        pffft_new_setup(int(FftSize), PFFFT_REAL), pffft_destroy_setup};
    detail::NativeRealFftLayout<FftSize> layout{setup.get()};
    struct Workspace {
        Buffer weighted = make_buffer();
        Buffer transformed = make_buffer();
        Buffer scratch = make_buffer();
        std::vector<float> power = std::vector<float>(bins);
    };
    std::array<Workspace, 2> workspaces;
    double window_power = 0.0;

    template<typename Phase, typename Prepare, typename Finish>
    auto density_impl(std::size_t input_size, scicpp::units::frequency<float> fs,
                      const Prepare& prepare, const Finish& finish) {
        using Density = scicpp::units::quantity_divide<
            scicpp::units::quantity_multiply<Phase, Phase>,
            scicpp::units::frequency<float>>;
        assert(input_size >= FftSize && fs.eval() > 0.0f);
        const auto segments = 1 + (input_size - FftSize) / (FftSize / 2);
        for (auto& workspace : workspaces)
            std::fill(workspace.power.begin(), workspace.power.end(), 0.0f);
        const auto process = [&](std::size_t worker) {
            auto& workspace = workspaces[worker];
            for (std::size_t segment = worker; segment < segments; segment += 2) {
                const auto offset = segment * (FftSize / 2);
                prepare(offset, workspace.weighted.get());
                pffft_transform(setup.get(), workspace.weighted.get(),
                    workspace.transformed.get(), workspace.scratch.get(), PFFFT_FORWARD);
                const auto* transformed = workspace.transformed.get();
                detail::accumulate_welch_power(transformed, workspace.power.data(), FftSize, layout.simd_size);
            }
        };
        std::future<void> background;
        if (segments > 1) background = std::async(std::launch::async, [&] {
            process(1);
            // With three Welch segments, this worker finishes one FFT while
            // acquisition runs two. Fill the phase snapshot in that spare time.
            finish();
        });
        process(0);
        if (background.valid()) background.get();
        else finish();
        const float scale = float(1.0 / (double(segments) * double(fs.eval()) * window_power));
        std::vector<Density> result(bins);
        for (std::size_t i = 0; i < bins; ++i) {
            const auto native = layout.positions[i];
            result[i] = Density{(workspaces[0].power[native] + workspaces[1].power[native]) * scale *
                (i == 0 || i == bins - 1 ? 1.0f : 2.0f)};
        }
        return result;
    }

  public:
    WelchSpectrum() {
        assert(setup);
        for (const auto value : window)
            window_power += double(value) * double(value);
    }

    template<class Array>
    auto density(const Array& input, scicpp::units::frequency<float> fs) {
        using Phase = typename Array::value_type;
        return density_impl<Phase>(input.size(), fs, [&](std::size_t offset, float* weighted) {
            double mean = 0.0;
            for (std::size_t i = 0; i < FftSize; ++i)
                mean += double(input[offset + i].eval());
            const float center = float(mean / double(FftSize));
            for (std::size_t i = 0; i < FftSize; ++i)
                weighted[i] = (input[offset + i].eval() - center) * window[i];
        }, [] {});
    }

    template<typename Phase, std::size_t N>
    auto density(const std::array<int32_t, N>& raw, const RawPhaseTrend& trend,
                 Phase radians_per_count, scicpp::units::frequency<float> fs,
                 std::array<Phase, N>* phase_snapshot = nullptr) {
        static_assert(N >= FftSize && FftSize <= 65536);
        const double center = double(FftSize - 1) / 2.0;
        const double scale = double(radians_per_count.eval());
        return density_impl<Phase>(N, fs, [&](std::size_t offset, float* weighted) {
            // Welch removes each segment's mean. Accumulate that mean exactly
            // in raw counts; the global fit's constant cancels. Preparing
            // segments here overlaps detrending with the other FFT worker.
            int64_t sum = 0;
            for (std::size_t i = 0; i < FftSize; ++i)
                sum += int64_t(raw[offset + i]);
            const double mean = double(sum) / double(FftSize);
            // Pipeline independent VFP operations on Cortex-A9 while keeping
            // each sample's double-precision subtraction and rounding order.
#pragma GCC unroll 4
            for (std::size_t i = 0; i < FftSize; ++i)
                weighted[i] = float((double(raw[offset + i]) - mean -
                    trend.slope * (double(i) - center)) * scale) * window[i];
        }, [&] {
            if (phase_snapshot) convert_relative_phase(raw, *phase_snapshot, radians_per_count);
        });
    }
};

} // namespace phase_noise
