#pragma once
#include "server/external_libs/pffft/pffft.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace phase_noise::detail {
// PR #772's plan-derived native-power permutation, shared by batch and streaming
// estimators. Real FFT reordering only permutes lanes; no conjugation is needed.
template<std::size_t FftSize>
struct NativeRealFftLayout {
    static constexpr std::size_t bins = FftSize / 2 + 1;
    static_assert(FftSize <= (1u << 24));
    using Index = std::conditional_t<(bins <= 65536), uint16_t, uint32_t>;
    const int simd_size = pffft_simd_size();
    std::array<Index, bins> positions{};
    explicit NativeRealFftLayout(const PFFFT_Setup* plan) {
        assert(plan && (simd_size == 1 || simd_size == 4));
        using Buffer = std::unique_ptr<float, decltype(&pffft_aligned_free)>;
        Buffer native{static_cast<float*>(pffft_aligned_malloc(FftSize * sizeof(float))), pffft_aligned_free};
        Buffer ordered{static_cast<float*>(pffft_aligned_malloc(FftSize * sizeof(float))), pffft_aligned_free};
        assert(native && ordered);
        for (std::size_t i = 0; i < FftSize; ++i) native.get()[i] = float(i);
        pffft_zreorder(plan, native.get(), ordered.get(), PFFFT_FORWARD);
        for (std::size_t i = 0; i < bins; ++i) {
            const auto lane = std::size_t(ordered.get()[i == bins - 1 ? 1 : 2 * i]);
            const auto index = simd_size == 1 ? (lane + 1) / 2 :
                (i == bins - 1 ? FftSize / 2 : (lane / 8) * 4 + lane % 8);
            assert(index < bins);
            positions[i] = Index(index);
        }
    }
};
}
