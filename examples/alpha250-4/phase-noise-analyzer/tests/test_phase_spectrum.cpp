#include "../phase-spectrum.hpp"
#include "../cumulative_averager.hpp"
#include <scicpp/signal.hpp>
#include <cassert>
#include <cmath>
#include <iostream>

using Phase = scicpp::units::radian<float>;
constexpr std::size_t samples = 32000;

int main() {
    std::array<Phase, samples> tone{}, shifted{}, ramp{};
    for (std::size_t i = 0; i < samples; ++i) {
        const double value = .01 * std::sin(2 * scicpp::pi<double> * 8 * double(i) / samples);
        tone[i] = Phase{float(value)};
        shifted[i] = Phase{float(4 + .001 * double(i) + value)};
        ramp[i] = Phase{float(4 + .001 * double(i))};
    }
    const auto residual = detrended_phase_prefix<samples>(ramp);
    double energy = 0.;
    for (auto value : residual) energy += double(value.eval()) * value.eval();
    assert(energy / samples < 1e-12);
    const auto a = detrended_phase_prefix<samples>(tone);
    const auto b = detrended_phase_prefix<samples>(shifted);
    for (std::size_t i = 0; i < samples; ++i)
        assert(std::abs(a[i].eval() - b[i].eval()) < 4e-6f);
    scicpp::signal::Spectrum<float> spectrum;
    spectrum.fs(1e6f);
    spectrum.window(scicpp::signal::windows::hann<float>(samples));
    const auto reference = spectrum.welch<scicpp::signal::SpectrumScaling::DENSITY, false>(tone);
    const auto clean = spectrum.welch<scicpp::signal::SpectrumScaling::DENSITY, false>(b);
    assert(std::abs(clean[8].eval() / reference[8].eval() - 1.f) < .001f);
    // Detrending changes the first few bins. Bound the response at the
    // minimum reported offset (two cycles/window), including worst-case sine.
    for (std::size_t i = 0; i < samples; ++i)
        tone[i] = Phase{float(.01 * std::sin(2 * scicpp::pi<double> * 2 * double(i) / samples))};
    const auto low_reference = spectrum.welch<scicpp::signal::SpectrumScaling::DENSITY, false>(tone);
    const auto low = spectrum.welch<scicpp::signal::SpectrumScaling::DENSITY, false>(detrended_phase_prefix<samples>(tone));
    const float ratio = low[2].eval() / low_reference[2].eval();
    std::cerr << "Two-bin detrending response: " << ratio << "\n";
    const double expected = std::pow(1.0 + 6.0 / (scicpp::pi<double> * scicpp::pi<double> * 4.0 * 3.0), 2);
    assert(std::abs(ratio - expected) < .001);
    // Exercise the actual production cross-spectrum and cumulative average.
    pna_spectrum::MultirateSpectrum cached_spectrum;
    const auto shared = pna_spectrum::cross_density(tone, tone,
        scicpp::units::frequency<float>{1e6f}, cached_spectrum);
    auto opposite = tone;
    for (auto& value : opposite) value = -value;
    const auto opposed = pna_spectrum::cross_density(tone, opposite,
        scicpp::units::frequency<float>{1e6f}, cached_spectrum);
    using Density = typename decltype(shared)::value_type;
    CumulativeAverager<Density> average;
    for (std::size_t i = 0; i < shared.size(); ++i) {
        assert(shared[i].real().eval() >= 0.f);
        assert(opposed[i].real().eval() <= 0.f);
        assert(std::abs(shared[i].real().eval() + opposed[i].real().eval()) < 1e-12f);
    }
    average.append(shared); average.append(opposed);
    assert(average.count() == 2);
    for (auto value : average.average())
        assert(std::abs(value.real().eval()) < 1e-12f && std::abs(value.imag().eval()) < 1e-12f);
    average.clear(); assert(average.count() == 0 && average.average().empty());
    for (int i = 0; i < 1000; ++i) average.append(opposed);
    assert(average.count() == 1000);
    const auto repeated = average.average();
    assert(std::abs(repeated[2].real().eval() / opposed[2].real().eval() - 1.f) < 2e-5f);

    // Spectral processing must leave the raw phase/slope used by telemetry intact.
    assert(std::abs(phase_slope_per_sample<samples>(ramp).eval() - .001f) < 1e-9f);
    std::cout << "Phase spectra reject affine drift and preserve modulation; two-bin response ratio " << ratio << '\n';
}
