#ifndef __SERVER_CORE_BUFFER_HPP__
#define __SERVER_CORE_BUFFER_HPP__

#include "server/network/serializer_deserializer.hpp"

#include <array>
#include <vector>
#include <tuple>
#include <string>
#include <cstdint>

namespace net {

template<size_t len>
struct Buffer
{
    explicit constexpr Buffer(size_t position_ = 0) noexcept
    : position(position_)
    {};

    constexpr size_t size() const {
        return len;
    }

    void set()     {_data.fill(0);}
    char* data()   {return _data.data();}
    char* begin()  {return &(_data.data())[position];}

    // These functions are used by Websocket

    template<typename... Tp>
    std::tuple<Tp...> deserialize() {
        static_assert(required_buffer_size<Tp...>() <= len, "Buffer size too small");

        const auto tup = net::deserialize<0, Tp...>(begin());
        position += required_buffer_size<Tp...>();
        return tup;
    }

    template<typename T, size_t N>
    const std::array<T, N>& extract_array() {
        // http://stackoverflow.com/questions/11205186/treat-c-cstyle-array-as-stdarray
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wcast-align"
        const auto p = reinterpret_cast<const std::array<T, N>*>(begin());
        #pragma GCC diagnostic pop
        // assert(p->data() == reinterpret_cast<const T*>(begin()));
        position += size_of<T, N>;
        return *p;
    }

    template<typename T>
    void to_container(std::vector<T>& vec, uint64_t length) {
        std::size_t n_elems = length / sizeof(T);
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wcast-align"
        const auto b = reinterpret_cast<const T*>(begin());
        #pragma GCC diagnostic pop
        vec.resize(n_elems);
        std::move(b, b + n_elems, vec.begin());
        position += length;
    }

    void to_container(std::string& str, uint64_t length) {
        str.resize(length);
        std::move(begin(), begin() + length, str.begin());
        position += length;
    }

  private:
    std::array<char, len> _data;
    size_t position; // Current position in the buffer
};

} // namespace net

#endif // __SERVER_CORE_BUFFER_HPP__
