#include "../analysis.hpp"
#include <cassert>
#include <iostream>
using namespace bist_full;
int main()
{
    constexpr unsigned n = 8192, bin = 208;
    std::vector<double> input(n);
    for (unsigned i = 0; i < n; ++i)
    {
        const double phase = 2 * std::numbers::pi * bin * i / n;
        input[i] = 200 + 3000 * std::cos(phase) + 30 * std::cos(2 * phase);
    }
    const auto r = single_tone(input, bin);
    assert(std::abs(r.amplitude - 3000) < 1e-7);
    assert(std::abs(r.sndr - 40) < 1e-6 && std::abs(r.thd + 40) < 1e-6 &&
           std::abs(r.sfdr - 40) < 1e-6);
    assert(std::abs(r.enob - (40 - 1.76) / 6.02) < 1e-6 && r.acceptable());
    const auto p = spectrum(input);
    const double power = std::accumulate(p.begin(), p.end(), 0.0);
    assert(std::abs(power - (200 * 200 + 3000 * 3000 / 2.0 + 30 * 30 / 2.0)) < 1e-5);
    for (unsigned i = 0; i < n; ++i)
    {
        const double phase = 2 * std::numbers::pi * bin * i / n;
        input[i] = std::clamp(6000 * std::cos(phase), -1500.0, 1500.0);
    }
    const auto clipped = single_tone(input, bin);
    assert(!clipped.acceptable() && clipped.thd > -25);
    for (unsigned i = 0; i < n; ++i)
        input[i] = 3000 * std::cos(2 * std::numbers::pi * bin * i / n) +
                   1000 * std::cos(2 * std::numbers::pi * 307 * i / n);
    assert(!single_tone(input, bin).acceptable());
    for (unsigned i = 0; i < n; ++i)
        input[i] = std::lround(7200 * std::cos(2 * std::numbers::pi * bin * i / n));
    assert(constant_bits(input) == 0);
    for (double &value : input)
        value = double(int32_t(std::lround(value)) & ~128);
    assert(constant_bits(input) == 128);
    std::fill(input.begin(), input.end(), 200);
    assert(!single_tone(input, bin).acceptable());
    for (unsigned i = 0; i < n; ++i)
        input[i] = (i & 1) ? -7 : 7;
    const auto nyquist = spectrum(input);
    assert(std::abs(nyquist[n / 2] - 49) < 1e-10);
    assert(spectrum({1, 2, 3}).empty());
    assert(!single_tone({}, 1).acceptable());
    // Harmonics that alias onto the same bin are counted once.
    for (unsigned i = 0; i < n; ++i)
        input[i] = 3000 * std::cos(2 * std::numbers::pi * 1024 * i / n) +
                   30 * std::cos(2 * std::numbers::pi * 2048 * i / n);
    assert(std::abs(single_tone(input, 1024).thd + 40) < 1e-6);
    std::cout << "PASS spectral analysis: absolute amplitude/power, harmonics, alias "
                 "deduplication, clipping, spurs, missing signal and Nyquist\n";
}
