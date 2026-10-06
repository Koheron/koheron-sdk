#pragma once
#include <array>
#include <chrono>
#include <tuple>

namespace phase_noise {
using StreamClock = std::chrono::steady_clock;
inline double elapsed_ms(StreamClock::time_point start) {
    return std::chrono::duration<double, std::milli>(StreamClock::now() - start).count();
}

// Smoothed service times exclude waiting for new DMA samples. Copying,
// estimation, averaging and publication all count toward processing capacity.
class StreamPerformance {
    std::array<double, 5> mean{};
    std::array<double, 5> fft_mean{};
    bool initialized = false;
 public:
    void reset() { mean = {}; fft_mean = {}; initialized = false; }
    void append(double total, double fft, double average, double publication, double copy,
                const std::array<double, 5>& fft_stages = {}) {
        const std::array<double, 5> next{total + copy, fft, average, publication, copy};
        for (std::size_t i = 0; i < mean.size(); ++i)
            mean[i] = initialized ? mean[i] + .05 * (next[i] - mean[i]) : next[i];
        for (std::size_t i = 0; i < fft_mean.size(); ++i)
            fft_mean[i] = initialized ? fft_mean[i] + .05 * (fft_stages[i] - fft_mean[i]) : fft_stages[i];
        initialized = true;
    }
    auto fft_status() const { return std::tuple{fft_mean[0], fft_mean[1], fft_mean[2], fft_mean[3], fft_mean[4]}; }
    auto status(double queue_ms, double retention_ms, double required_hz) const {
        return std::tuple{mean[0], mean[1], mean[2], mean[3], mean[4],
            queue_ms, retention_ms, required_hz, mean[0] > 0 ? 1000.0 / mean[0] : 0.0};
    }
};
}
