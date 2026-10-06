#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <tuple>

namespace phase_noise {
// Union of accepted FFT sample intervals. Pending DMA samples are excluded
// until accepted or skipped; overlapping windows never count samples twice.
class StreamCoverage {
    uint64_t epoch = 0, first = 0, last = 0, last_first = 0, covered = 0;
    bool initialized = false;
 public:
    void reset() {
        ++epoch;
        first = last = last_first = covered = 0;
        initialized = false;
    }
    void append(uint64_t end, uint32_t chunks) {
        assert(chunks > 0 && end >= chunks);
        if (!initialized) {
            first = end - chunks;
            covered = chunks;
            initialized = true;
        } else {
            // A repeated or stale window adds no new sample coverage.
            if (end <= last) return;
            // Window starts are chronological even when single-channel seed
            // windows cover three FFTs instead of the steady-state final FFT.
            assert(end - chunks >= last_first);
            covered += std::min<uint64_t>(chunks, end - last);
        }
        last = end;
        last_first = end - chunks;
    }
    auto status() const { return std::tuple{epoch, covered, initialized ? last - first : uint64_t{0}}; }
};
}
