#ifndef __ENDIAN_UTILS_HPP__
#define __ENDIAN_UTILS_HPP__

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace koheron {

//------------------------------------------------------------------------------
// Byteswap
//------------------------------------------------------------------------------

constexpr bool host_is_be = (std::endian::native == std::endian::big);

constexpr uint16_t bswap16(uint16_t x) noexcept { return __builtin_bswap16(x); }
constexpr uint32_t bswap32(uint32_t x) noexcept { return __builtin_bswap32(x); }
constexpr uint64_t bswap64(uint64_t x) noexcept { return __builtin_bswap64(x); }

template<class T>
constexpr T to_be(T v) noexcept {
    if constexpr (sizeof(T) == 1) return v;
    else if constexpr (sizeof(T) == 2) return host_is_be ? v : static_cast<T>(bswap16(static_cast<uint16_t>(v)));
    else if constexpr (sizeof(T) == 4) return host_is_be ? v : static_cast<T>(bswap32(static_cast<uint32_t>(v)));
    else if constexpr (sizeof(T) == 8) return host_is_be ? v : static_cast<T>(bswap64(static_cast<uint64_t>(v)));
    else static_assert(sizeof(T) <= 8, "Unsupported size");
}

template<class T>
constexpr T from_be(T v) noexcept { return to_be(v); }

} // namespace koheron

#endif // __ENDIAN_UTILS_HPP__