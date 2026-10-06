#include "server/external_libs/pffft/pffft.h"
#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iostream>
#include <memory>
#include <numbers>
#include <vector>

namespace {
struct Free {
    void operator()(float* p) const { pffft_aligned_free(p); }
};
using Buffer = std::unique_ptr<float[], Free>;
Buffer allocate(std::size_t count) {
    Buffer result{static_cast<float*>(pffft_aligned_malloc(count * sizeof(float)))};
    assert(result);
    return result;
}

void check(int n, pffft_transform_t type) {
    const auto count = std::size_t(n) * (type == PFFFT_COMPLEX ? 2U : 1U);
    std::unique_ptr<PFFFT_Setup, decltype(&pffft_destroy_setup)> setup{
        pffft_new_setup(n, type), pffft_destroy_setup};
    assert(setup);
    auto input = allocate(count), native = allocate(count), ordered = allocate(count);
    auto reordered = allocate(count), scratch = allocate(count), inverse = allocate(count);
    for (int pattern = 0; pattern < 4; ++pattern) {
        uint32_t state = 0x9e3779b9U;
        double energy = 0.;
        float maximum = 0.f;
        for (std::size_t i = 0; i < count; ++i) {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            const float random = float(std::bit_cast<int32_t>(state)) / float(INT32_MAX);
            input[i] = pattern == 0 ? (i == 7 ? 1.f : 0.f) :
                pattern == 1 ? (i % 2 ? 1.f : -1.f) :
                pattern == 2 ? random : random * 1e-30f;
            energy += double(input[i]) * double(input[i]);
            maximum = std::max(maximum, std::abs(input[i]));
        }
        pffft_transform(setup.get(), input.get(), native.get(), scratch.get(), PFFFT_FORWARD);
        pffft_zreorder(setup.get(), native.get(), reordered.get(), PFFFT_FORWARD);
        pffft_transform_ordered(setup.get(), input.get(), ordered.get(), scratch.get(), PFFFT_FORWARD);
        for (std::size_t i = 0; i < count; ++i)
            assert(std::bit_cast<uint32_t>(ordered[i]) == std::bit_cast<uint32_t>(reordered[i]));

        // Independent double-precision DFT covers radix-2/3/4/5 and the
        // specialized complex N=32 codelet, including real DC/Nyquist packing.
        if (n <= 160 && pattern != 3) {
            const int bins = type == PFFFT_REAL ? n / 2 + 1 : n;
            for (int k = 0; k < bins; ++k) {
                std::complex<double> reference{};
                for (int i = 0; i < n; ++i) {
                    const auto value = type == PFFFT_REAL ?
                        std::complex<double>{input[std::size_t(i)], 0.} :
                        std::complex<double>{input[std::size_t(2*i)], input[std::size_t(2*i+1)]};
                    const double angle = -2. * std::numbers::pi * double(k) * double(i) / double(n);
                    reference += value * std::complex<double>{std::cos(angle), std::sin(angle)};
                }
                const auto actual = type == PFFFT_REAL && (k == 0 || k == n/2) ?
                    std::complex<double>{ordered[k == 0 ? 0U : 1U], 0.} :
                    std::complex<double>{ordered[std::size_t(2*k)], ordered[std::size_t(2*k+1)]};
                assert(std::abs(reference - actual) <= 2e-5 * (1. + std::abs(reference)));
            }
        }
        double spectral_energy = 0.;
        for (std::size_t i = 0; i < count; ++i) {
            const double weight = type == PFFFT_REAL && i > 1 ? 2. : 1.;
            spectral_energy += weight * double(ordered[i]) * double(ordered[i]);
        }
        assert(std::abs(spectral_energy / double(n) - energy) <= 2e-5 * energy);

        for (bool canonical : {false, true}) {
            auto transform = canonical ? pffft_transform_ordered : pffft_transform;
            const auto* spectrum = canonical ? ordered.get() : native.get();
            transform(setup.get(), spectrum, inverse.get(), scratch.get(), PFFFT_BACKWARD);
            const float tolerance = 3e-5f * maximum;
            for (std::size_t i = 0; i < count; ++i)
                assert(std::abs(inverse[i] / float(n) - input[i]) <= tolerance);
            std::copy_n(input.get(), count, inverse.get());
            transform(setup.get(), inverse.get(), inverse.get(), scratch.get(), PFFFT_FORWARD);
            for (std::size_t i = 0; i < count; ++i)
                assert(std::bit_cast<uint32_t>(inverse[i]) == std::bit_cast<uint32_t>(spectrum[i]));
            transform(setup.get(), inverse.get(), inverse.get(), scratch.get(), PFFFT_BACKWARD);
            for (std::size_t i = 0; i < count; ++i)
                assert(std::abs(inverse[i] / float(n) - input[i]) <= tolerance);
        }
    }
}
}

int main() {
    for (int n : {32, 64, 96, 128, 160, 288, 512, 800, 2048, 8192, 32768})
        check(n, PFFFT_REAL);
    for (int n : {16, 32, 48, 64, 80, 144, 240, 512, 960, 30000})
        check(n, PFFFT_COMPLEX);
    std::cout << "PFFFT " << pffft_simd_arch()
              << ": DFT, Parseval, ordering and in-place forward/inverse checks passed.\n";
}
