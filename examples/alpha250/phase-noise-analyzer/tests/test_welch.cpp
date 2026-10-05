// Run natively and on ARM to cover the NEON implementation and VFP fallback.
#include "server/drivers/phase-noise/welch-spectrum.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

static void check_power() {
    constexpr std::size_t size = 32768;
    std::array<float, size> transformed{};
    std::array<float, size / 2 + 1> expected{}, actual{};
    uint32_t random = 7;
    for (std::size_t i = 0; i < size; ++i) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        transformed[i] = float(int32_t(random)) / 2147483648.f;
    }
    // Include underflowing products, a previously accumulated subnormal,
    // exact threshold values, nonfinite values and the scalar tail.
    for (std::size_t i = 8; i < 32; ++i) transformed[i] = std::ldexp(1.f, -70);
    transformed[64] = transformed[65] = std::ldexp(1.f, -63);
    transformed[100] = std::numeric_limits<float>::infinity();
    transformed[120] = std::numeric_limits<float>::quiet_NaN();
    transformed[size - 2] = std::ldexp(1.f, -70);
    transformed[size - 1] = -std::ldexp(1.f, -70);
    expected[20] = std::ldexp(1.f, -140);
    actual = expected;
    for (int pass = 0; pass < 3; ++pass) {
        expected[0] += transformed[0] * transformed[0];
        expected[size / 2] += transformed[1] * transformed[1];
        for (std::size_t i = 1; i < size / 2; ++i)
            expected[i] += transformed[2 * i] * transformed[2 * i] +
                           transformed[2 * i + 1] * transformed[2 * i + 1];
        phase_noise::detail::accumulate_welch_power(transformed.data(), actual.data(), size);
        for (std::size_t i = 0; i < actual.size(); ++i) {
            if (std::isnan(expected[i])) assert(std::isnan(actual[i]));
            else assert(std::bit_cast<uint32_t>(actual[i]) == std::bit_cast<uint32_t>(expected[i]));
        }
    }
    assert(actual[5] > 0.f && actual[5] < std::numeric_limits<float>::min());
}

template<std::size_t FftSize, std::size_t Samples>
static void check_snapshot() {
    using Phase = scicpp::units::radian<float>;
    std::array<int32_t, Samples> raw{};
    uint32_t random = 11;
    for (auto& value : raw) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        value = int32_t(random);
    }
    phase_noise::WelchSpectrum<FftSize> estimator;
    const Phase scale{1e-5f};
    const scicpp::units::frequency<float> fs{12500000.f};
    std::array<Phase, Samples> expected{}, snapshot{};
    for (const int32_t origin : {INT32_MIN, 0, INT32_MAX}) {
        raw.front() = origin;
        const auto trend = phase_noise::fit_raw_phase_prefix<Samples>(raw);
        convert_relative_phase(raw, expected, scale);
        const auto baseline = estimator.density(raw, trend, scale, fs);
        const auto concurrent = estimator.density(raw, trend, scale, fs, &snapshot);
        assert(baseline == concurrent);
        assert(snapshot == expected);
        assert(snapshot.front().eval() == 0.f);
    }
}

int main() {
    check_power();
    check_snapshot<32, 32>(); // One Welch segment: conversion stays synchronous.
    check_snapshot<32, 48>(); // Two equally sized FFT workloads.
    check_snapshot<32768, 65536>(); // Production: conversion uses the shorter worker.
#if defined(__ARM_NEON)
    std::cout << "ARM NEON/VFP power, subnormals and concurrent phase snapshots: PASS\n";
#else
    std::cout << "Scalar power and concurrent phase snapshots: PASS\n";
#endif
}
