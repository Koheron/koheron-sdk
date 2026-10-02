#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace prm {
constexpr uint32_t n_pts = 262144;
constexpr uint32_t cic_decimation_rate_min = 4, cic_decimation_rate_max = 8192;
constexpr uint32_t cic_decimation_rate_default = 20;
constexpr uint32_t cic_n_stages = 6, cic_differential_delay = 1;
}
namespace mem { enum {control, status, ram}; }
namespace reg {
constexpr uint32_t phase_incr0 = 0, cordic = 16, cic_rate = 20;
constexpr uint32_t demod0 = 0, demod1 = 4;
}
namespace hw {
template<int id> class Memory {
public:
    std::array<std::atomic<uint32_t>, 16> words{};
    std::atomic<unsigned> reads{0}, writes{0};
    // Only modify the stimulus while the fake DMA is waiting.
    int32_t origin = 2000000000;
    double drift = 0.001, amplitude = 0.1;
    template<uint32_t offset, class T = uint32_t> T read() { return words[offset / 4].load(); }
    template<class T> void write_reg(uint32_t offset, T value) {
        words[offset / 4].store(static_cast<uint32_t>(value));
        if constexpr (sizeof(T) == 8) words[offset / 4 + 1].store(static_cast<uint32_t>(value >> 32));
        ++writes;
    }
    template<uint32_t offset> void write(uint32_t value) { write_reg(offset, value); }
    template<uint32_t offset, uint32_t mask> void write_mask(uint32_t value) {
        words[offset / 4].store((words[offset / 4].load() & ~mask) | (value & mask));
        ++writes;
    }
    template<class T, uint32_t N, uint32_t offset> auto read_array() {
        static_assert(id == mem::ram && offset == 98304);
        ++reads;
        std::array<T, N> result{};
        // Exactly one representable count of drift per sample; known bin-64 PM.
        for (uint32_t i = 0; i < N; ++i)
            result[i] = origin + static_cast<int32_t>(std::llround(
                (drift * i + amplitude * std::sin(2.0 * 3.141592653589793 * 64 * i / 32768)) / (3.141592653589793 / 2048)));
        return result;
    }
};
template<int id> Memory<id>& get_memory() { static Memory<id> value; return value; }
}
