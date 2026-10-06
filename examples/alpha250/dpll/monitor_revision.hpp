#pragma once
#include <cstdint>
#include <mutex>
#include <shared_mutex>

namespace dpll_monitor {
// Serializes a settings snapshot with loop edits, never with a DMA wait or FFT.
inline std::shared_mutex controls_mutex;
inline uint64_t controls_revision = 0;
class ControlChange {
    std::unique_lock<std::shared_mutex> lock{controls_mutex};
  public:
    ~ControlChange() { ++controls_revision; }
};
}
