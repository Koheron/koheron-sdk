/// (c) Koheron

#include "server/core/session.hpp"

namespace koheron {

// -----------------------------------------------
// TCP
// -----------------------------------------------

template<> int Session<TCP>::init_socket() {return 0;}
template<> int Session<TCP>::exit_socket() {return 0;}

template<>
int Session<TCP>::read_command(Command& cmd) {
    // Read and decode header
    // |      RESERVED     | dev_id  |  op_id  |             payload_size              |   payload
    // |  0 |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | ...
    const int header_bytes = read_exact(
        comm_fd,
        std::as_writable_bytes(std::span{cmd.header.data(), cmd.header.size()})
    );

    if (header_bytes == 0) {
        return header_bytes;
    }

    if (header_bytes < 0) {
        log<ERROR>("TCPSocket: Cannot read header\n");
        return header_bytes;
    }

    const auto [drv_id, op] = cmd.header.deserialize<uint16_t, uint16_t>();
    cmd.session_id = id;
    cmd.session = this;
    cmd.socket_type = TCP;
    cmd.comm_fd = comm_fd;
    cmd.driver = static_cast<driver_id>(drv_id);
    cmd.operation = op;

    logf<DEBUG>("TCPSocket: Receive command for driver {}, operation {}\n",
                cmd.driver, cmd.operation);

    return header_bytes;
}

// -----------------------------------------------
// WebSocket
// -----------------------------------------------

template<>
int Session<WEBSOCK>::init_socket() {
    websock.set_id(comm_fd);

    if (websock.authenticate() < 0) {
        log<CRITICAL>("Cannot connect websocket to client\n");
        return -1;
    }

    return 0;
}

template<> int Session<WEBSOCK>::exit_socket() {
    return websock.exit();
}

template<>
int Session<WEBSOCK>::read_command(Command& cmd) {
    if (websock.receive_cmd(cmd.header, cmd.payload) < 0) {
        log<ERROR>("WebSocket: Command reception failed\n");
        return -1;
    }

    if (websock.is_closed()) {
        return 0;
    }

    if (websock.payload_size() < Command::HEADER_SIZE) {
        log<ERROR>("WebSocket: Command too small\n");
        return -1;
    }

    const auto [drv_id, op] = cmd.header.deserialize<uint16_t, uint16_t>();
    cmd.session_id = id;
    cmd.session = this;
    cmd.socket_type = WEBSOCK;
    cmd.comm_fd = comm_fd;
    cmd.driver = static_cast<driver_id>(drv_id);
    cmd.operation = op;

    logf<DEBUG>("WebSocket: Receive command for driver {}, operation {}\n",
                cmd.driver, cmd.operation);

    return Command::HEADER_SIZE;
}

} // namespace koheron
