#ifndef KOHERON_MULTIRATE_DECIMATION_HPP
#define KOHERON_MULTIRATE_DECIMATION_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <type_traits>
#include <scicpp/core.hpp>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace pna_dsp {
namespace sci = scicpp;
inline constexpr float fir_cutoff = 0.030f;
inline constexpr std::size_t fir_ntaps = 161;

template <std::size_t Ntaps>
constexpr auto make_lowpass_fir(float cutoff) {
    static_assert(Ntaps % 2 == 1);

    std::array<float, Ntaps> h{};

    constexpr std::size_t center = Ntaps / 2;
    float sum = 0.0f;

    for (std::size_t n = 0; n < Ntaps; ++n) {
        const int m = int(n) - int(center);

        float sinc;
        if (n == center) {
            sinc = 2.0f * cutoff;
        } else {
            sinc = std::sin(2.0f * sci::pi<float> * cutoff * float(m))
                 / (sci::pi<float> * float(m));
        }

        const float w = 0.42f
                      - 0.5f * std::cos(2.0f * sci::pi<float> * float(n) / float(Ntaps - 1))
                      + 0.08f * std::cos(4.0f * sci::pi<float> * float(n) / float(Ntaps - 1));

        h[n] = sinc * w;
        sum += h[n];
    }

    for (auto& v : h) {
        v /= sum;
    }

    return h;
}

template <typename T, std::size_t M, std::size_t N, std::size_t Ntaps = fir_ntaps>
auto decimate_by_10_fir_exact(const std::array<T, N>& in) {
    static_assert(Ntaps % 2 == 1);

    constexpr auto b = make_lowpass_fir<Ntaps>(fir_cutoff);
    constexpr std::size_t delay = Ntaps / 2;
    static_assert(10 * M + delay <= N);
    std::array<T, M> out{};
#if defined(__ARM_NEON)
    // Convert quantities once, then evaluate retained FIR outputs four taps
    // at a time. Keep startup zero-history handling identical to the scalar
    // implementation, and retain the original coefficients and sample times.
    std::array<float, N> samples{};
    for (std::size_t i = 0; i < N; ++i) {
        if constexpr (sci::units::is_quantity_v<T>) samples[i] = in[i].eval();
        else samples[i] = in[i];
    }
    constexpr auto reversed = [&] {
        std::array<float, Ntaps> result{};
        for (std::size_t j = 0; j < Ntaps; ++j) result[j] = b[Ntaps - 1 - j];
        return result;
    }();
#endif
    // Evaluate only retained outputs, with the same zero history and FIR delay.
    for (std::size_t i = 0; i < M; ++i) {
        const std::size_t sample = 10 * i + delay;
        const std::size_t taps = std::min(Ntaps, sample + 1);
#if defined(__ARM_NEON)
        if (taps == Ntaps && std::is_same_v<sci::units::representation_t<T>, float>) {
            const std::size_t first = sample - (Ntaps - 1);
            auto sum = vdupq_n_f32(0.0f);
            std::size_t j = 0;
            for (; j + 4 <= Ntaps; j += 4)
                sum = vaddq_f32(sum, vmulq_f32(vld1q_f32(samples.data() + first + j),
                                               vld1q_f32(reversed.data() + j)));
            const auto halves = vadd_f32(vget_low_f32(sum), vget_high_f32(sum));
            float value = vget_lane_f32(halves, 0) + vget_lane_f32(halves, 1);
            for (; j < Ntaps; ++j) value += samples[first + j] * reversed[j];
            out[i] = T{value};
            continue;
        }
#endif
        for (std::size_t j = 0; j < taps; ++j) {
            out[i] += in[sample - j] * b[j];
        }
    }

    return out;
}

template <std::size_t Ntaps = fir_ntaps>
float decimate_by_10_fir_mag2(sci::units::dimensionless<float> f_norm) {
    constexpr auto h = make_lowpass_fir<Ntaps>(fir_cutoff);
    constexpr auto pi = sci::pi<sci::units::radian<float>>;

    std::complex<float> H{0.0f, 0.0f};

    for (std::size_t n = 0; n < Ntaps; ++n) {
        const auto phi = -2.0f * pi * f_norm * float(n);
        H += h[n] * std::complex<float>{sci::cos(phi), sci::sin(phi)};
    }

    return std::norm(H);
}


template<std::size_t Size, std::size_t Level>
const auto& decimation_compensation() {
    static_assert(Size > 1 && Level > 0);
    // Normalized frequencies depend on FFT sizes and decimation ratios, not fs.
    static const auto weights = [] {
        std::array<float, Size> out{};
        out.fill(1.0f);
        for (std::size_t k = 1; k < Size; ++k) {
            float gain = 1.0f;
            for (std::size_t stage = 0; stage < Level; ++stage) {
                const float ratio = std::pow(10.0f, float(Level - stage));
                const float normalized = float(k) / (float(2 * (Size - 1)) * ratio);
                gain *= decimate_by_10_fir_mag2(sci::units::dimensionless<float>{normalized});
            }
            if (gain > 0.80f) out[k] = 1.0f / gain;
        }
        return out;
    }();
    return weights;
}

} // namespace pna_dsp
#endif
