/// Commands received by the server
///
/// (c) Koheron

#ifndef __COMMANDS_HPP__
#define __COMMANDS_HPP__

#include <array>
#include <vector>
#include <tuple>
#include <string>

#include <drivers_table.hpp>
#include "server_definitions.hpp"
#include "serializer_deserializer.hpp"

namespace koheron {

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

        const auto tup = koheron::deserialize<0, Tp...>(begin());
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
    void to_vector(std::vector<T>& vec, uint64_t length) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wcast-align"
        const auto b = reinterpret_cast<const T*>(begin());
        #pragma GCC diagnostic pop
        vec.resize(length);
        std::move(b, b + length, vec.begin());
        position += length * sizeof(T);
    }

    void to_string(std::string& str, uint64_t length) {
        str.resize(length);
        std::move(begin(), begin() + length, str.begin());
        position += length;
    }

  private:
    std::array<char, len> _data;
    size_t position; // Current position in the buffer
};

class SessionAbstract;

struct Command
{
    Command() noexcept
    : header(HEADER_START)
    {}

    enum Header : uint32_t {
        HEADER_SIZE = 8,
        HEADER_START = 4  // First 4 bytes are reserved
    };

    SessionID session_id = -1; // ID of the session emitting the command
    SessionAbstract *session; // Pointer to the session emitting the command
    driver_id driver = driver_id_of<NoDriver>; // The driver to control
    int32_t operation = -1; // Operation ID

    Buffer<HEADER_SIZE> header; // Raw data header
    Buffer<CMD_PAYLOAD_BUFFER_LEN> payload;
};

} // namespace koheron

#endif // __COMMANDS_HPP__

