#include "server/drivers/fft/spectrum.hpp"
#include <cassert>
#include <complex>
#include <limits>
#include <numbers>

int main() {
    std::array<float, 4> factors{1, 2, 3, 4};
    assert(fft::valid_power_conversion(2.0, factors));
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    for (const double range : {0.0, double(nan), double(INFINITY)}) {
        assert(!fft::valid_power_conversion(range, factors));
    }
    for (const float invalid : {0.0f, -1.0f, nan, float(INFINITY)}) {
        factors[2] = invalid;
        assert(!fft::valid_power_conversion(2.0, factors));
    }

    // Independent DFT of the documented fixed-to-float input. Parseval must
    // give one full-scale sine's power for every window/rate/average count.
    constexpr size_t n = 256, bin = 17;
    constexpr double amplitude = 16384.0;
    for (const bool hann : {false, true}) {
        std::array<double, n> data{};
        double sum_window = 0, sum_square_window = 0;
        for (size_t i = 0; i < n; ++i) {
            const double phase = 2 * std::numbers::pi * i / n;
            const double window = hann ? .5 - .5 * std::cos(phase) : 1;
            sum_window += window; sum_square_window += window * window;
            data[i] = amplitude * std::sin(bin * phase) * window;
        }
        const double w1 = std::pow(sum_window / n, 2), w2 = sum_square_window / n;
        for (const double rate : {125e6, 250e6}) {
            for (const uint32_t averages : {1u, 1023u}) {
                std::array<float, n/2> raw{};
                for (size_t k = 0; k < raw.size(); ++k) {
                    std::complex<double> dft{};
                    for (size_t i = 0; i < n; ++i) {
                        dft += data[i] * std::polar(1.0, -2 * std::numbers::pi * k * i / n);
                    }
                    raw[k] = float(std::norm(dft) * averages);
                }
                const auto density = fft::full_scale_density(raw, averages, rate, w2);
                double integrated = 0;
                for (const float value : density) { integrated += value * rate / n; }
                const double tone = density[bin] * (w2 / w1) * rate / n;
                assert(std::abs(integrated - 1.0) < 2e-7);
                assert(std::abs(10 * std::log10(tone)) < 1e-6);
                // At half amplitude the power must fall by 6.0206 dB.
                for (auto& value : raw) { value *= .25f; }
                const auto half = fft::full_scale_density(raw, averages, rate, w2);
                assert(std::abs(10 * std::log10(half[bin] / density[bin]) + 6.020599913) < 1e-6);
            }
        }
    }
    // DC has no negative-frequency partner: a full-scale DC input has twice
    // the mean-square power of the full-scale sine reference, rather than four.
    std::array<float, n/2> dc{};
    dc[0] = float(std::pow(amplitude * n, 2));
    const auto density = fft::full_scale_density(dc, 1, 250e6, 1);
    assert(std::abs(density[0] * 250e6 / n - 2) < 1e-7);
}
