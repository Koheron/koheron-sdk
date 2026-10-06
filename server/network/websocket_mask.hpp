#ifndef KOHERON_WEBSOCKET_MASK_HPP
#define KOHERON_WEBSOCKET_MASK_HPP

#include <cstddef>
#include <cstdint>

namespace net::detail {

inline void unmask_scalar(const uint8_t* src, uint8_t* dst, std::size_t size,
                          const uint8_t* mask, std::size_t phase) {
    for (std::size_t i = 0; i < size; ++i) {
        dst[i] = src[i] ^ mask[phase & 3];
        ++phase;
    }
}

// ARM GCC can isolate NEON instructions without changing the server's FPU flags.
#if defined(__linux__) && defined(__arm__) && defined(__GNUC__) && !defined(__clang__) && defined(__ARM_FP) && __ARM_ARCH >= 7
#define KOHERON_WEBSOCKET_RUNTIME_NEON 1
#endif

#if defined(KOHERON_WEBSOCKET_RUNTIME_NEON) || defined(__ARM_NEON)
void unmask_bulk(const uint8_t* src, uint8_t* dst, std::size_t size,
                 const uint8_t* mask, std::size_t phase);
#endif

// Source and destination must be disjoint, or exactly equal. No alignment required.
inline void unmask(const uint8_t* src, uint8_t* dst, std::size_t size,
                   const uint8_t* mask, std::size_t phase = 0) {
#if defined(KOHERON_WEBSOCKET_RUNTIME_NEON) || defined(__ARM_NEON)
    // Keep command headers and small arguments on the inexpensive inline path.
    if (size >= 64) {
        unmask_bulk(src, dst, size, mask, phase);
        return;
    }
#endif
    unmask_scalar(src, dst, size, mask, phase);
}

} // namespace net::detail

#endif
