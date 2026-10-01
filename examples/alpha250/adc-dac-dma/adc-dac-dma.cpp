/// DMA driver bode processing helpers
///
/// (c) Koheron

#include "./adc-dac-dma.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>

#include <scicpp/signal/fft.hpp>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

std::vector<double> make_hann_window(std::size_t n) {
    std::vector<double> window(n);
    if (n == 0) {
        return window;
    }
    if (n == 1) {
        window[0] = 1.0;
        return window;
    }
    const double denom = static_cast<double>(n - 1);
    for (std::size_t i = 0; i < n; ++i) {
        window[i] = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i) / denom);
    }
    return window;
}

std::vector<double> unwrap_phase(const std::vector<std::complex<double>>& h) {
    std::vector<double> phase(h.size());
    if (h.empty()) {
        return phase;
    }
    for (std::size_t i = 0; i < h.size(); ++i) {
        phase[i] = std::atan2(h[i].imag(), h[i].real());
    }
    double offset = 0.0;
    for (std::size_t i = 1; i < phase.size(); ++i) {
        const double delta = phase[i] - phase[i - 1];
        if (delta > kPi) {
            offset -= kTwoPi;
        } else if (delta < -kPi) {
            offset += kTwoPi;
        }
        phase[i] += offset;
    }
    return phase;
}

double estimate_delay(const std::vector<std::complex<double>>& h,
                      const std::vector<double>& freqs,
                      const std::vector<uint8_t>& mask,
                      double band_lo,
                      double band_hi) {
    if (h.size() != freqs.size()) {
        return 0.0;
    }

    const auto phase = unwrap_phase(h);
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xx = 0.0;
    double sum_xy = 0.0;
    std::size_t count = 0;

    for (std::size_t i = 0; i < freqs.size(); ++i) {
        const double f = freqs[i];
        if (f < band_lo || f > band_hi) {
            continue;
        }
        if (!mask.empty() && mask[i] == 0) {
            continue;
        }
        const double ph = phase[i];
        sum_x += f;
        sum_y += ph;
        sum_xx += f * f;
        sum_xy += f * ph;
        ++count;
    }

    if (count < 10) {
        return 0.0;
    }

    const double denom = static_cast<double>(count) * sum_xx - sum_x * sum_x;
    if (std::abs(denom) < 1e-20) {
        return 0.0;
    }
    const double slope = (static_cast<double>(count) * sum_xy - sum_x * sum_y) / denom;
    return -slope / kTwoPi;
}

void unpack_samples(std::span<const uint32_t> packed,
                    std::vector<double>& out,
                    std::size_t n_samples) {
    out.assign(n_samples, 0.0);
    std::size_t out_idx = 0;
    for (std::size_t i = 0; i < packed.size() && out_idx < n_samples; ++i) {
        const uint32_t word = packed[i];
        const int16_t lo = static_cast<int16_t>(word & 0xFFFF);
        const int16_t hi = static_cast<int16_t>(word >> 16);
        out[out_idx++] = static_cast<double>(lo);
        if (out_idx < n_samples) {
            out[out_idx++] = static_cast<double>(hi);
        }
    }
}

} // namespace

void AdcDacDma::bode_reset(uint32_t n_fft, double fs_hz) {
    if (n_fft == 0) {
        n_fft = 1;
    }
    bode_state.n_fft = n_fft;
    bode_state.fs = fs_hz;
    bode_state.window = make_hann_window(n_fft);
    bode_state.freqs = scicpp::signal::rfftfreq<double>(n_fft, 1.0 / fs_hz);
    bode_state.sxx.assign(bode_state.freqs.size(), 0.0);
    bode_state.syx.assign(bode_state.freqs.size(), std::complex<double>{0.0, 0.0});
    bode_state.h_real.assign(bode_state.freqs.size(), 0.0);
    bode_state.h_imag.assign(bode_state.freqs.size(), 0.0);
    bode_state.mask.assign(bode_state.freqs.size(), 0);
    bode_state.count = 0;
    bode_state.last_tau = 0.0;
}

void AdcDacDma::bode_set_baseline(const std::vector<double>& h_real,
                                  const std::vector<double>& h_imag,
                                  const std::vector<uint8_t>& mask) {
    const std::size_t size = h_real.size();
    if (size == 0 || h_imag.size() != size) {
        bode_state.baseline_valid = false;
        bode_state.baseline.clear();
        bode_state.baseline_mask.clear();
        return;
    }

    bode_state.baseline.resize(size);
    for (std::size_t i = 0; i < size; ++i) {
        bode_state.baseline[i] = std::complex<double>{h_real[i], h_imag[i]};
    }
    bode_state.baseline_mask = mask;
    if (!bode_state.baseline_mask.empty() && bode_state.baseline_mask.size() != size) {
        bode_state.baseline_mask.assign(size, 1);
    }
    bode_state.baseline_valid = true;
}

void AdcDacDma::bode_clear_baseline() {
    bode_state.baseline_valid = false;
    bode_state.baseline.clear();
    bode_state.baseline_mask.clear();
}

void AdcDacDma::bode_acquire_step(uint32_t N,
                                  double thr_rel,
                                  bool remove_delay,
                                  double band_lo,
                                  double band_hi,
                                  bool apply_baseline) {
    if (bode_state.n_fft == 0) {
        return;
    }
    if (N < 1) {
        N = 1;
    }
    if (N > n_desc) {
        N = n_desc;
    }

    start_dma(N);
    auto adc_data = ram_s2mm.read_span<uint32_t>(N * n_pts);
    auto dac_data = ram_mm2s.read_span<uint32_t>(N * n_pts);
    stop_dma();

    std::vector<double> x;
    std::vector<double> y;
    const std::size_t n_fft = bode_state.n_fft;
    unpack_samples(dac_data, x, n_fft);
    unpack_samples(adc_data, y, n_fft);

    for (std::size_t i = 0; i < n_fft; ++i) {
        x[i] *= bode_state.window[i];
        y[i] *= bode_state.window[i];
    }

    auto X = scicpp::signal::rfft(x);
    auto Y = scicpp::signal::rfft(y);
    const std::size_t n_freq = X.size();

    if (bode_state.sxx.size() != n_freq) {
        bode_state.sxx.assign(n_freq, 0.0);
        bode_state.syx.assign(n_freq, std::complex<double>{0.0, 0.0});
        bode_state.h_real.assign(n_freq, 0.0);
        bode_state.h_imag.assign(n_freq, 0.0);
        bode_state.mask.assign(n_freq, 0);
    }

    for (std::size_t i = 0; i < n_freq; ++i) {
        const double x_power = std::norm(X[i]);
        const std::complex<double> yx = Y[i] * std::conj(X[i]);
        const double inv = 1.0 / static_cast<double>(bode_state.count + 1);
        bode_state.sxx[i] = (bode_state.sxx[i] * bode_state.count + x_power) * inv;
        bode_state.syx[i] = (bode_state.syx[i] * static_cast<double>(bode_state.count) + yx) * inv;
    }

    bode_state.count += 1;

    const double sxx_max = bode_state.sxx.empty()
        ? 0.0
        : *std::max_element(bode_state.sxx.begin(), bode_state.sxx.end());

    std::vector<std::complex<double>> H(bode_state.sxx.size());
    bode_state.mask.assign(bode_state.sxx.size(), 0);
    for (std::size_t i = 0; i < bode_state.sxx.size(); ++i) {
        const double denom = bode_state.sxx[i] + 1e-30;
        H[i] = bode_state.syx[i] / denom;
        if (bode_state.sxx[i] > thr_rel * sxx_max) {
            bode_state.mask[i] = 1;
        }
    }

    bode_state.last_tau = 0.0;
    if (remove_delay) {
        bode_state.last_tau = estimate_delay(H, bode_state.freqs, bode_state.mask, band_lo, band_hi);
        if (std::abs(bode_state.last_tau) > 0.0) {
            for (std::size_t i = 0; i < H.size(); ++i) {
                const double phase = kTwoPi * bode_state.freqs[i] * bode_state.last_tau;
                H[i] *= std::exp(std::complex<double>{0.0, phase});
            }
        }
    }

    if (apply_baseline && bode_state.baseline_valid && bode_state.baseline.size() == H.size()) {
        for (std::size_t i = 0; i < H.size(); ++i) {
            if (!bode_state.baseline_mask.empty() && bode_state.baseline_mask[i] == 0) {
                H[i] = std::complex<double>{
                    std::numeric_limits<double>::quiet_NaN(),
                    std::numeric_limits<double>::quiet_NaN()
                };
                continue;
            }
            const double mag = std::abs(bode_state.baseline[i]);
            if (mag <= 1e-12) {
                H[i] = std::complex<double>{
                    std::numeric_limits<double>::quiet_NaN(),
                    std::numeric_limits<double>::quiet_NaN()
                };
                continue;
            }
            H[i] /= bode_state.baseline[i];
        }
    }

    bode_state.h_real.resize(H.size());
    bode_state.h_imag.resize(H.size());
    for (std::size_t i = 0; i < H.size(); ++i) {
        bode_state.h_real[i] = H[i].real();
        bode_state.h_imag[i] = H[i].imag();
    }
}
