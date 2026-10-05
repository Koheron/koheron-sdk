#ifndef ALPHA250_4_PHASE_SPECTRUM_HPP
#define ALPHA250_4_PHASE_SPECTRUM_HPP

#include "phase-decimation.hpp"
#include "phase-processing.hpp"
#include "server/drivers/phase-noise/single-window-spectrum.hpp"
#include <array>
#include <tuple>
#include <scicpp/signal.hpp>

// Shared by the instrument and independent numerical regression checks.
namespace pna_spectrum {
namespace sci = scicpp;
namespace sig = scicpp::signal;

using namespace pna_dsp;

constexpr std::size_t fft_decimation_steps = 2;
constexpr float stitch_fraction = 0.2f;

constexpr std::size_t fir_delay = fir_ntaps / 2;

// Choose final /100 length.
// Needs: 100 * d2_size + 11 * fir_delay <= base_size
constexpr std::size_t decimated2_size = 300;
constexpr std::size_t decimated1_size = 10 * decimated2_size;
constexpr std::size_t decimated0_size = 10 * decimated1_size;

struct MultirateSpectrum {
    phase_noise::SingleWindowSpectrum full{decimated0_size};
    phase_noise::SingleWindowSpectrum middle{decimated1_size};
    phase_noise::SingleWindowSpectrum low{decimated2_size};
};

// Temporary d1 must be longer so second decimation can discard FIR delay.
constexpr std::size_t decimated1_tmp_size = decimated1_size + fir_delay;

// First-stage input needed to produce decimated1_tmp_size.
constexpr std::size_t decimation_input_size =
    10 * decimated1_tmp_size + fir_delay;

static_assert(decimation_input_size <= 32000);

template <typename T, std::size_t M, std::size_t N>
auto take_prefix(const std::array<T, N>& in) {
    static_assert(M <= N);

    std::array<T, M> out{};

    for (std::size_t i = 0; i < M; ++i) {
        out[i] = in[i];
    }

    return out;
}

template <std::size_t Steps, typename T, std::size_t N>
auto build_decimation_chain(const std::array<T, N>& input) {
    static_assert(Steps == 2, "This exact-duration chain is currently written for two decimation stages.");
    static_assert(decimation_input_size <= N);

    // x0, x1, x2 have exactly the same time duration:
    // x0: 30000 samples @ fs
    // x1: 3000 samples @ fs / 10
    // x2: 300 samples @ fs / 100
    auto x0_for_filter = take_prefix<T, decimation_input_size>(input);

    auto x1_tmp = decimate_by_10_fir_exact<T, decimated1_tmp_size>(x0_for_filter);
    auto x2     = decimate_by_10_fir_exact<T, decimated2_size>(x1_tmp);

    auto x0 = take_prefix<T, decimated0_size>(input);
    auto x1 = take_prefix<T, decimated1_size>(x1_tmp);

    return std::tuple{x0, x1, x2};
}

template <typename Arr>
auto welch_density(sig::Spectrum<float>& sp,
                   Arr& data,
                   sci::units::frequency<float> fs) {
    sp.fs(fs);
    sp.window(sig::windows::hann<float>(data.size()));
    return sp.welch<sig::SpectrumScaling::DENSITY, false>(data);
}

template <typename ArrX, typename ArrY>
auto csd_density(sig::Spectrum<float>& sp,
                 ArrX& x,
                 ArrY& y,
                 sci::units::frequency<float> fs) {
    sp.fs(fs);
    sp.window(sig::windows::hann<float>(x.size()));
    return sp.csd<sig::SpectrumScaling::DENSITY, false>(x, y);
}

template<std::size_t Level, typename SpectrumLike>
void compensate_decimated_psd(SpectrumLike& spectrum) {
    constexpr auto size = Level == 1 ? decimated1_size / 2 + 1 : decimated2_size / 2 + 1;
    const auto& weights = decimation_compensation<size, Level>();
    for (std::size_t k = 1; k < spectrum.size(); ++k) spectrum[k] = spectrum[k] * weights[k];
}

template <typename Spectrum0, typename Spectrum1, typename Spectrum2>
auto stitch_segments(const Spectrum0& s0,
                     const Spectrum1& s1,
                     const Spectrum2& s2) {
    auto out = s0;

    // Since x0/x1/x2 have exactly equal duration, df is identical.
    // Therefore bin-index stitching is valid again.
    const auto k21 = std::size_t(stitch_fraction * float(s2.size()));
    const auto k10 = std::size_t(stitch_fraction * float(s1.size()));

    for (std::size_t k = 0; k < out.size(); ++k) {
        if (k < k21) {
            out[k] = s2[k];
        } else if (k < k10) {
            out[k] = s1[k];
        } else {
            out[k] = s0[k];
        }
    }

    return out;
}


template <typename Phase, std::size_t N>
auto auto_density(const std::array<Phase, N>& input,
                  sci::units::frequency<float> fs, MultirateSpectrum& spectrum) {
    auto [x0, x1, x2] = build_decimation_chain<fft_decimation_steps>(input);
    auto s0 = spectrum.full.density(x0, fs);
    auto s1 = spectrum.middle.density(x1, fs / 10.0f);
    auto s2 = spectrum.low.density(x2, fs / 100.0f);
    compensate_decimated_psd<1>(s1);
    compensate_decimated_psd<2>(s2);
    return stitch_segments(s0, s1, s2);
}

template <typename Phase, std::size_t N>
auto cross_density(const std::array<Phase, N>& x, const std::array<Phase, N>& y,
                   sci::units::frequency<float> fs, MultirateSpectrum& spectrum, bool remove_drift = true) {
    constexpr std::size_t base_size = 32000;
    auto x0 = remove_drift ? detrended_phase_prefix<base_size>(x) : take_prefix<Phase, base_size>(x);
    auto y0 = remove_drift ? detrended_phase_prefix<base_size>(y) : take_prefix<Phase, base_size>(y);
    auto [dx0, dx1, dx2] = build_decimation_chain<fft_decimation_steps>(x0);
    auto [dy0, dy1, dy2] = build_decimation_chain<fft_decimation_steps>(y0);
    auto s0 = spectrum.full.cross_density(dx0, dy0, fs);
    auto s1 = spectrum.middle.cross_density(dx1, dy1, fs / 10.0f);
    auto s2 = spectrum.low.cross_density(dx2, dy2, fs / 100.0f);
    compensate_decimated_psd<1>(s1);
    compensate_decimated_psd<2>(s2);
    return stitch_segments(s0, s1, s2);
}
} // namespace pna_spectrum
#endif
