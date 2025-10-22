#ifndef __SERVER_CORE_BUFFER_HPP__
#define __SERVER_CORE_BUFFER_HPP__

#include "server/network/serializer_deserializer.hpp"

#include <array>
#include <vector>
#include <tuple>
#include <string>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <cstring>

namespace net {

template<std::size_t len>
struct Buffer
{
    explicit constexpr Buffer(std::size_t position_ = 0) noexcept
    : position(position_)
    {}

    constexpr std::size_t size() const noexcept {
        return len;
    }

    void set() noexcept {_data.fill(std::byte{0});}

    std::byte* data() noexcept { return _data.data(); }
    const std::byte* data() const noexcept { return _data.data(); }

    std::byte* begin() noexcept { return _data.data() + position; }
    const std::byte* begin() const noexcept { return _data.data() + position; }

    // These functions are used by Websocket

    template<typename... Tp>
    std::tuple<Tp...> deserialize() {
        static_assert(required_buffer_size<Tp...>() <= len, "Buffer size too small");

        const auto tup = net::deserialize<0, Tp...>(begin());
        position += required_buffer_size<Tp...>();
        return tup;
    }

    template<typename T>
    void to_container(std::vector<T>& vec, uint64_t length) {
        static_assert(std::is_trivially_copyable_v<T>);
        std::size_t n_elems = length / sizeof(T);
        vec.resize(n_elems);
        std::memcpy(vec.data(), begin(), n_elems * sizeof(T));
        position += length;
    }

    void to_container(std::string& str, uint64_t length) {
        str.resize(length);
        std::memcpy(str.data(), reinterpret_cast<const char*>(begin()), length);
        position += length;
    }

  private:
    std::array<std::byte, len> _data;
    std::size_t position; // Current position in the buffer
};

} // namespace net

#endif // __SERVER_CORE_BUFFER_HPP__
