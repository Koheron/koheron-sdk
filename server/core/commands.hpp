/// Commands received by the server
///
/// (c) Koheron

#ifndef __COMMANDS_HPP__
#define __COMMANDS_HPP__

#include "server/runtime/drivers_table.hpp"
#include "server/core/configs/server_definitions.hpp"
#include "server/core/buffer.hpp"
#include "server/core/session_abstract.hpp"
#include "server/utilities/concepts.hpp"

#include <cstdint>
#include <utility>
#include <initializer_list>

namespace koheron {

// class SessionAbstract;

class Command
{
  public:
    Command() noexcept
    : header(HEADER_START)
    {}

    template <class Tuple, std::size_t... I>
    bool read_arguments(Tuple& args, std::index_sequence<I...>) {
        bool ok = true;
        (void)std::initializer_list<int>{ (ok = ok && read_one(std::get<I>(args)), 0)... };
        return ok;
    }

    enum Header : uint32_t {
        HEADER_SIZE = 8,
        HEADER_START = 4  // First 4 bytes are reserved
    };

    SessionID session_id = -1;           // ID of the session emitting the command
    SessionAbstract *session = nullptr;  // Pointer to the session emitting the command
    driver_id driver = 0;                // The driver to control
    int32_t operation = -1;              // Operation ID

    Buffer<HEADER_SIZE> header; // Raw data header
    Buffer<CMD_PAYLOAD_BUFFER_LEN> payload;

  private:
    template <typename T>
    bool read_one(T& v) {
        if constexpr (resizableContiguousRange<T>) {
            return session->template recv(v, payload) >= 0;
        } else { // fixed-size / POD-ish types
            auto [status, value] = session->template deserialize<T>(payload);

            if (status < 0) {
                return false;
            }

            v = value;
            return true;
        }
    }
};

} // namespace koheron

#endif // __COMMANDS_HPP__
