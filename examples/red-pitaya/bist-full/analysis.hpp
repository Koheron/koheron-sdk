#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>
#include <numeric>
#include <vector>

namespace bist_full
{
// Coherent, rectangular-window spectrum. Callers choose integer cycles and reject sample loss.
inline std::vector<double> spectrum(const std::vector<double> &samples)
{
    const size_t n = samples.size();
    if (n < 2 || (n & (n - 1)))
        return {};
    std::vector<std::complex<double>> a(samples.begin(), samples.end());
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t length = 2; length <= n; length <<= 1)
    {
        const auto factor = std::polar(1.0, -2 * std::numbers::pi / double(length));
        for (size_t i = 0; i < n; i += length)
        {
            std::complex<double> w{1, 0};
            for (size_t j = 0; j < length / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + length / 2] * w;
                a[i + j] = u + v;
                a[i + j + length / 2] = u - v;
                w *= factor;
            }
        }
    }
    std::vector<double> p(n / 2 + 1);
    for (size_t i = 0; i < p.size(); ++i)
        p[i] = std::norm(a[i]) * (i == 0 || i == n / 2 ? 1 : 2) / double(n * n);
    return p;
}
inline double db(double numerator, double denominator)
{
    return 10 * std::log10(std::max(numerator, 1e-30) / std::max(denominator, 1e-30));
}
inline uint32_t constant_bits(const std::vector<double> &samples)
{
    uint32_t any_one = 0, all_one = 0x3fff;
    for (double sample : samples)
    {
        const auto code = uint32_t(std::lround(sample)) & 0x3fff;
        any_one |= code;
        all_one &= code;
    }
    return (~any_one | all_one) & 0x3fff;
}
struct Spectral
{
    double amplitude = 0, sndr = 0, snr = 0, thd = 0, sfdr = 0, enob = 0;
    bool acceptable() const { return amplitude > 100 && sndr > 35 && thd < -30 && sfdr > 35; }
};
inline Spectral single_tone(const std::vector<double> &samples, size_t bin)
{
    const auto p = spectrum(samples);
    if (bin == 0 || bin + 1 >= p.size())
        return {};
    const double signal = p[bin];
    double distortion = 0, spur = 0, rest = 0;
    std::vector<size_t> harmonics;
    for (size_t h = 2; h <= 10; ++h)
    {
        size_t b = (h * bin) % samples.size();
        if (b > samples.size() / 2)
            b = samples.size() - b;
        if (b && b != bin && std::find(harmonics.begin(), harmonics.end(), b) == harmonics.end())
        {
            harmonics.push_back(b);
            distortion += p[b];
        }
    }
    for (size_t i = 1; i < p.size(); ++i)
        if (i != bin)
        {
            rest += p[i];
            spur = std::max(spur, p[i]);
        }
    const double sndr = db(signal, rest);
    return {std::sqrt(2 * signal),
            sndr,
            db(signal, std::max(0.0, rest - distortion)),
            db(distortion, signal),
            db(signal, spur),
            (sndr - 1.76) / 6.02};
}
} // namespace bist_full
