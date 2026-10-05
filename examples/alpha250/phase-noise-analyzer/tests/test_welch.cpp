// Run natively and on ARM to cover the NEON implementation and VFP fallback.
#include "server/drivers/phase-noise/welch-spectrum.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

static void check_power() {
    constexpr std::size_t size = 32768;
    alignas(16) std::array<float, size> transformed{}, native{}, packed{}, ordered{};
    std::array<float, size / 2 + 1> expected{}, power{};
    auto* setup = pffft_new_setup(size, PFFFT_REAL);
    assert(setup);
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
    // Use PFFFT's own permutation as an independent layout oracle. Keep the
    // scalar ordered calculation below independent of native lane packing.
    ordered[40] = expected[20];
    pffft_zreorder(setup, ordered.data(), packed.data(), PFFFT_BACKWARD);
    const auto unpack_power = [&] {
        if (pffft_simd_size() == 1) {
            power[0] = packed[0];
            power[size / 2] = packed[size - 1];
            for (std::size_t i = 1; i < size / 2; ++i) power[i] = packed[2 * i - 1];
        } else {
            for (std::size_t i = 0; i < size; i += 8)
                for (std::size_t lane = 0; lane < 4; ++lane) power[i / 2 + lane] = packed[i + lane];
            power[size / 2] = packed[4];
        }
    };
    unpack_power();
    pffft_zreorder(setup, transformed.data(), native.data(), PFFFT_BACKWARD);
    for (int pass = 0; pass < 3; ++pass) {
        expected[0] += transformed[0] * transformed[0];
        expected[size / 2] += transformed[1] * transformed[1];
        for (std::size_t i = 1; i < size / 2; ++i)
            expected[i] += transformed[2 * i] * transformed[2 * i] +
                           transformed[2 * i + 1] * transformed[2 * i + 1];
        phase_noise::detail::accumulate_welch_power(native.data(), power.data(), size, pffft_simd_size());
        std::fill(packed.begin(), packed.end(), 0.f);
        if (pffft_simd_size() == 1) {
            packed[0] = power[0];
            packed[size - 1] = power[size / 2];
            for (std::size_t i = 1; i < size / 2; ++i) packed[2 * i - 1] = power[i];
        } else {
            for (std::size_t i = 0; i < size; i += 8)
                for (std::size_t lane = 0; lane < 4; ++lane) packed[i + lane] = power[i / 2 + lane];
            packed[4] = power[size / 2];
        }
        pffft_zreorder(setup, packed.data(), ordered.data(), PFFFT_FORWARD);
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const auto actual = ordered[i == size / 2 ? 1 : 2 * i];
            if (std::isnan(expected[i])) assert(std::isnan(actual));
            else assert(std::bit_cast<uint32_t>(actual) == std::bit_cast<uint32_t>(expected[i]));
        }
    }
    assert(ordered[10] > 0.f && ordered[10] < std::numeric_limits<float>::min());
    pffft_destroy_setup(setup);
}

template<std::size_t FftSize, std::size_t Samples>
static auto ordered_reference(const std::array<int32_t, Samples>& raw,
                              const phase_noise::RawPhaseTrend& trend,
                              scicpp::units::radian<float> scale,
                              scicpp::units::frequency<float> fs) {
    // The previous algorithm: canonical FFT output for every segment, followed
    // by scalar power accumulation. Independent of the native layout helper
    // and the estimator's cached publication permutation.
    const auto window = scicpp::signal::windows::hann<float>(FftSize);
    double window_power = 0.;
    for (auto value : window) window_power += double(value) * double(value);
    constexpr auto segments = 1 + (Samples - FftSize) / (FftSize / 2);
    alignas(16) std::array<float, FftSize> weighted{}, transformed{}, scratch{};
    std::array<std::array<float, FftSize / 2 + 1>, 2> power{};
    auto* setup = pffft_new_setup(FftSize, PFFFT_REAL);
    assert(setup);
    for (std::size_t segment = 0; segment < segments; ++segment) {
        const auto offset = segment * (FftSize / 2);
        int64_t sum = 0;
        for (std::size_t i = 0; i < FftSize; ++i) sum += int64_t(raw[offset + i]);
        const double mean = double(sum) / double(FftSize);
        for (std::size_t i = 0; i < FftSize; ++i)
            weighted[i] = float((double(raw[offset + i]) - mean -
                trend.slope * (double(i) - double(FftSize - 1) / 2.)) * double(scale.eval())) * window[i];
        pffft_transform_ordered(setup, weighted.data(), transformed.data(), scratch.data(), PFFFT_FORWARD);
        auto& accumulated = power[segment % 2];
        accumulated.front() += transformed[0] * transformed[0];
        accumulated.back() += transformed[1] * transformed[1];
        for (std::size_t i = 1; i < FftSize / 2; ++i)
            accumulated[i] += transformed[2 * i] * transformed[2 * i] +
                              transformed[2 * i + 1] * transformed[2 * i + 1];
    }
    pffft_destroy_setup(setup);
    const float normalization = float(1. / (double(segments) * double(fs.eval()) * window_power));
    std::array<float, FftSize / 2 + 1> result{};
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = (power[0][i] + power[1][i]) * normalization *
                    (i == 0 || i == FftSize / 2 ? 1.f : 2.f);
    return result;
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
        const auto reference = ordered_reference<FftSize>(raw, trend, scale, fs);
        for (std::size_t i = 0; i < baseline.size(); ++i)
            assert(std::bit_cast<uint32_t>(baseline[i].eval()) == std::bit_cast<uint32_t>(reference[i]));
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
    check_snapshot<64, 192>(); // Five segments test repeated accumulation in both workers.
    check_snapshot<32768, 65536>(); // Production: conversion uses the shorter worker.
#if defined(__ARM_NEON)
    std::cout << "ARM NEON/VFP power, subnormals and concurrent phase snapshots: PASS\n";
#else
    std::cout << "Scalar power and concurrent phase snapshots: PASS\n";
#endif
}
