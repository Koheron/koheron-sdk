#ifndef KOHERON_VNA_SWEEP_PLAN_HPP
#define KOHERON_VNA_SWEEP_PLAN_HPP
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace vna {
constexpr uint32_t capture_samples = 64 * 1024 * 1024;
constexpr uint32_t packet_bytes = 256 * 1024;
constexpr uint32_t max_batch_blocks = 16;
// Whole DMA packets, power-of-two FFT size, and enough room after excitation.
inline bool valid_record(uint32_t samples, uint32_t chirp_samples) {
    return samples >= 4 * 1024 * 1024 && samples <= capture_samples &&
           (samples & (samples - 1)) == 0 && chirp_samples <= samples - 1024;
}
constexpr double sample_rate = 250000000.0;
struct SweepPlan {
    uint32_t samples{}, gain{};
    uint64_t coefficient{};
    std::array<uint64_t, 16> seeds{};
};
inline bool make_plan(double start, double stop, double duration, double level_dbfs, SweepPlan& plan, double rate = sample_rate) {
    if (!std::isfinite(start) || !std::isfinite(stop) || !std::isfinite(duration) ||
        !std::isfinite(level_dbfs) || start < 100 || stop <= start || stop >= rate / 2 ||
        duration <= 2.0 * (1u << 18) / rate ||
        duration > double(capture_samples - 1024) / rate || level_dbfs < -40 || level_dbfs > 0)
        return false;
    const auto samples = uint32_t(std::llround(duration * rate));
    if (samples <= 2 * (1u << 18) || samples > capture_samples - 1024) return false;
    const long double step = std::log(static_cast<long double>(stop) / start) / (samples - 1);
    const long double scale = std::ldexp(1.0L, 64);
    const long double coefficient = std::round(std::expm1(16 * step) * scale);
    if (coefficient < 1 || coefficient >= std::ldexp(1.0L, 48)) return false;
    plan.samples = samples;
    plan.gain = uint32_t(std::min(32767.0, std::round(32768.0 * std::pow(10.0, level_dbfs / 20))));
    plan.coefficient = uint64_t(coefficient);
    for (unsigned i = 0; i < 16; ++i) {
        const long double value = std::round(start / rate * scale * std::exp(i * step));
        if (value < 1 || value >= std::ldexp(1.0L, 63)) return false;
        plan.seeds[i] = uint64_t(value);
    }
    return true;
}
}
#endif
