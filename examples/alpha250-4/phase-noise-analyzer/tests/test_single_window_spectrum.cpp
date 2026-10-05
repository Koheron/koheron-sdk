#include "server/drivers/phase-noise/single-window-spectrum.hpp"
#include <scicpp/signal.hpp>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

using Phase = scicpp::units::radian<float>;
using Frequency = scicpp::units::frequency<float>;

void check_size(std::size_t size) {
    phase_noise::SingleWindowSpectrum cached(size);
    scicpp::signal::Spectrum<float> reference;
    reference.window(scicpp::signal::windows::hann<float>(size));
    std::vector<Phase> x(size), y(size);
    std::mt19937 random(764);
    std::normal_distribution<float> noise(0.f, .001f);
    for (std::size_t i = 0; i < size; ++i) {
        const double angle = 2 * scicpp::pi<double> * 7 * double(i) / double(size);
        x[i] = Phase{float(.01 * std::sin(angle) + noise(random))};
        y[i] = Phase{float(.012 * std::cos(angle) + noise(random))};
    }
    // Reuse the same plans through both sample-rate and auto/cross changes.
    for (float fs : {751879.7f, 1492537.3f, 1e5f}) {
        reference.fs(fs);
        for (float gain : {1.f, 1e-6f, 1e6f}) {
            auto other = y;
            for (auto& value : other) value *= gain;
            const auto expected = reference.csd<scicpp::signal::SpectrumScaling::DENSITY, false>(x, other);
            const auto actual = cached.cross_density(x, other, Frequency{fs});
            double error = 0., energy = 0.;
            for (std::size_t k = 0; k < actual.size(); ++k) {
                const std::complex<double> a{actual[k].real().eval(), actual[k].imag().eval()};
                const std::complex<double> b{expected[k].real().eval(), expected[k].imag().eval()};
                error += std::norm(a - b); energy += std::norm(b);
            }
            assert(std::sqrt(error / energy) < 3e-5);
        }
        const auto expected = reference.welch<scicpp::signal::SpectrumScaling::DENSITY, false>(x);
        const auto actual = cached.density(x, Frequency{fs});
        const auto shared = cached.cross_density(x, x, Frequency{fs});
        auto opposite = x;
        for (auto& value : opposite) value = -value;
        const auto opposed = cached.cross_density(x, opposite, Frequency{fs});
        double error = 0., energy = 0.;
        for (std::size_t k = 0; k < actual.size(); ++k) {
            const double difference = actual[k].eval() - expected[k].eval();
            error += difference * difference; energy += double(expected[k].eval()) * expected[k].eval();
            assert(shared[k].real().eval() >= 0.f && shared[k].imag().eval() == 0.f);
            assert(opposed[k].real().eval() <= 0.f && opposed[k].imag().eval() == 0.f);
            assert(shared[k].real().eval() == -opposed[k].real().eval());
        }
        assert(std::sqrt(error / energy) < 3e-5);
    }
    // DC detrending, one-sided endpoint scaling, and no stale work buffers.
    std::fill(x.begin(), x.end(), Phase{.125f});
    for (auto value : cached.density(x, Frequency{1e6f})) assert(value.eval() == 0.f);
    for (std::size_t i = 0; i < size; ++i) x[i] = Phase{i % 2 ? -.01f : .01f};
    reference.fs(1e6f);
    const auto expected = reference.welch<scicpp::signal::SpectrumScaling::DENSITY, false>(x);
    const auto actual = cached.density(x, Frequency{1e6f});
    assert(std::abs(actual.back().eval() / expected.back().eval() - 1.f) < 3e-5f);
}

int main() {
    for (std::size_t size : {30000, 3000, 300, 301}) check_size(size);
    std::cout << "Cached periodograms preserve signed CSD, six decades of channel ratio, rate changes and endpoints\n";
}
