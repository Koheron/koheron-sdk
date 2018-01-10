/// Server definitions and static configurations
///
/// (c) Koheron

#ifndef __SERVER_DEFINITIONS_HPP__
#define __SERVER_DEFINITIONS_HPP__

#include <cstdint>
#include <array>
#include <string>

namespace koheron {

// ------------------------------------------
// Connections
// ------------------------------------------

/// Pending connections queue size
constexpr int NUMBER_OF_PENDING_CONNECTIONS = 10;

// ------------------------------------------
// Buffer sizes
// ------------------------------------------

/// Number of samples
constexpr int KOHERON_SIG_LEN = 16384;

/// Command payload buffer length
constexpr int64_t CMD_PAYLOAD_BUFFER_LEN = 16384 * 16;

/// Read string length
constexpr int KOHERON_READ_STR_LEN = 16384;

/// Send string length
constexpr int KOHERON_SEND_STR_LEN = 16384;

/// Receive data buffer length
constexpr int KOHERON_RECV_DATA_BUFF_LEN = 16384 * 16;

/// Websocket receive buffer size
constexpr int WEBSOCK_READ_STR_LEN = KOHERON_RECV_DATA_BUFF_LEN;

/// Websocket send buffer size (bytes)
constexpr int WEBSOCK_SEND_BUF_LEN = 16384 * 16;

// ------------------------------------------
// Debugging
// ------------------------------------------

#ifndef DEBUG_KOHERON
# define NDEBUG
#endif

// ------------------------------------------
// Misc
// ------------------------------------------

using SessionID = int;

/// Listening channel types
enum SockType {
    NONE,
    TCP,
    WEBSOCK,
    UNIX,
    socket_type_num
};

/// Listening channel descriptions
const std::array<std::string, socket_type_num>
listen_channel_desc = {{
    "NONE",
    "TCP",
    "WebSocket",
    "Unix socket"
}};

} // namespace koheron

#endif // __SERVER_DEFINITIONS_HPP__


