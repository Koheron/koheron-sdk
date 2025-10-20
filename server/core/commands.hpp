/// Commands received by the server
///
/// (c) Koheron

#ifndef __COMMANDS_HPP__
#define __COMMANDS_HPP__

#include "server/runtime/drivers_table.hpp"
#include "server/core/configs/server_definitions.hpp"
#include "server/core/buffer.hpp"

#include <cstdint>

namespace koheron {

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

    SessionID session_id = -1;           // ID of the session emitting the command
    SessionAbstract *session = nullptr;  // Pointer to the session emitting the command
    driver_id driver = 0;                // The driver to control
    int32_t operation = -1;              // Operation ID

    Buffer<HEADER_SIZE> header; // Raw data header
    Buffer<CMD_PAYLOAD_BUFFER_LEN> payload;
};

} // namespace koheron

#endif // __COMMANDS_HPP__
