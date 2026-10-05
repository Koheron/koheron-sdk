#ifndef PNA_ACQUISITION_WINDOW_HPP
#define PNA_ACQUISITION_WINDOW_HPP

#include <cstdint>
#include <optional>

struct AcquisitionWindow {
    uint64_t first_chunk;
    uint64_t end_chunk;
};

// Each accepted window contains only samples newer than the previous window.
inline std::optional<AcquisitionWindow> acquisition_window(
    uint64_t completed, uint64_t consumed, uint32_t chunks) {
    if (completed < consumed || completed - consumed < chunks) {
        return std::nullopt;
    }
    return AcquisitionWindow{completed - chunks, completed};
}

inline bool acquisition_window_is_intact(
    AcquisitionWindow window, uint64_t completed, uint32_t ring_chunks) {
    // The producer can already be writing the next, unpublished chunk.
    return completed >= window.end_chunk && completed - window.first_chunk < ring_chunks;
}

#endif
