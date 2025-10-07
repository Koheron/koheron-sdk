#ifndef __SERVER_RUNTIME_ENDIAN_UTILS_HPP__
#define __SERVER_RUNTIME_ENDIAN_UTILS_HPP__

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace koheron {

//------------------------------------------------------------------------------
// Byteswap + Host endianness helpers
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

//------------------------------------------------------------------------------
// Container pack
//------------------------------------------------------------------------------

template <typename T, typename OutputIt>
constexpr void to_big_endian_bytes(T value, OutputIt out, std::size_t len) noexcept {
    static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>, "Unsigned integral only");
    // Caller ensures len range; we keep it branchless/constexpr.
    for (std::size_t i = 0; i < len; ++i) {
        *out++ = static_cast<std::uint8_t>(value >> (8 * (len - 1 - i)));
    }
}

template <typename T>
constexpr bool to_big_endian_bytes(T value, std::span<std::uint8_t> out, std::size_t len) noexcept {
    static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>, "Unsigned integral only");
    if (len > out.size() || len == 0 || len > sizeof(T)) return false;
    to_big_endian_bytes<T>(value, out.begin(), len);
    return true;
}

template <std::size_t N, typename T>
constexpr bool to_big_endian_bytes(T value, std::array<std::uint8_t, N>& out,
                                   std::size_t len, std::size_t offset = 0) noexcept {
    static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>, "Unsigned integral only");
    if (offset > N || len == 0 || len > sizeof(T) || (offset + len) > N) return false;
    to_big_endian_bytes<T>(value, out.begin() + static_cast<std::ptrdiff_t>(offset), len);
    return true;
}

//------------------------------------------------------------------------------
// Container unpack
//------------------------------------------------------------------------------

template <typename T, typename InputIt>
[[nodiscard]] constexpr T from_big_endian_bytes(InputIt in, std::size_t len) noexcept {
    static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>, "Unsigned integral only");
    T v = 0;
    for (std::size_t i = 0; i < len; ++i) {
        v = static_cast<T>((v << 8) | static_cast<T>(*in++));
    }
    return v;
}

template <typename T>
[[nodiscard]] constexpr T from_big_endian_bytes(std::span<const std::uint8_t> in, std::size_t len) noexcept {
    static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>, "Unsigned integral only");
    // Caller should ensure len <= in.size() and len <= sizeof(T)
    return from_big_endian_bytes<T>(in.begin(), len);
}

template <std::size_t N, typename T>
[[nodiscard]] constexpr T from_big_endian_bytes(const std::array<std::uint8_t, N>& in,
                                                std::size_t len, std::size_t offset = 0) noexcept {
    static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>, "Unsigned integral only");
    // Caller should ensure (offset+len) <= N and len <= sizeof(T)
    return from_big_endian_bytes<T>(in.begin() + static_cast<std::ptrdiff_t>(offset), len);
}

} // namespace koheron

#endif // __SERVER_RUNTIME_ENDIAN_UTILS_HPP__