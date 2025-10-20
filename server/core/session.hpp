/// (c) Koheron

#ifndef __KOHERON_SESSION_HPP__
#define __KOHERON_SESSION_HPP__

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/utilities/concepts.hpp"

#include "server/core/configs/server_definitions.hpp"
#include "server/core/commands.hpp"
#include "server/core/session_abstract.hpp"
#include "server/core/drivers/driver_executor.hpp"
#include "server/core/websocket.hpp"

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unistd.h>
#include <type_traits>
#include <cassert>
#include <memory_resource>
#include <ranges>
#include <span>
#include <cstddef>
#include <cerrno>
#include <sys/socket.h>

namespace koheron {

/// Session
///
/// Receive and parse the client request for execution
template<int socket_type>
class Session : public SessionAbstract
{
  public:
    Session(int comm_fd, SessionID id_);

    int run();

    SessionID get_id() const {return id;}

    void shutdown() override {
        if (::shutdown(comm_fd, SHUT_RDWR) < 0) {
            logf<WARNING>("Cannot shutdown socket for session ID: {}\n", id);
        }

        ::close(comm_fd);
    }

  private:
    int comm_fd;  ///< Socket file descriptor
    SessionID id;

    struct EmptyWebsock {
        EmptyWebsock() {}
    };

    std::conditional_t<socket_type == WEBSOCK, WebSocket, EmptyWebsock> websock;

    int init_socket();
    int exit_socket();

    int read_command(Command& cmd);

    template<std::ranges::contiguous_range R>
    int write(const R& r);

    int write_bytes(std::span<const std::byte> bytes) override {
        // R = std::span<const std::byte>
        return write(bytes);
    }
};

template<int socket_type>
Session<socket_type>::Session(int comm_fd_, SessionID id_)
: SessionAbstract(socket_type)
, comm_fd(comm_fd_)
, id(id_)
, websock{}
{}

template<int socket_type>
int Session<socket_type>::run() {
    if (init_socket() < 0) {
        return -1;
    }

    while (!exit_signal) {
        Command cmd;
        const int nb_bytes_rcvd = read_command(cmd);

        if (exit_signal) {
            break;
        }

        if (nb_bytes_rcvd <= 0) {
            // We don't call exit_session() here because the socket is already closed.
            return nb_bytes_rcvd;
        }

        if (services::require<DriverExecutor>().execute(cmd) < 0) {
            logf<ERROR>("Failed to execute command [driver = {}, operation = {}]\n",
                        cmd.driver, cmd.operation);
        }

        if (status == CLOSED) {
            break;
        }
    }

    if (exit_socket() < 0) {
        log<WARNING>("An error occured during session exit\n");
    }

    return 0;
}

template<int socket_type>
template<std::ranges::contiguous_range R>
int Session<socket_type>::write(const R& r) {
    if constexpr (socket_type == TCP || socket_type == UNIX) {
        using T = std::remove_cvref_t<std::ranges::range_value_t<R>>;

        const auto bytes_send = sizeof(T) * std::size(r);
        const int n_bytes_send = ::write(comm_fd, std::data(r), bytes_send);

        if (n_bytes_send == 0) {
            log<ERROR>("TCPSocket::write: Connection closed by client\n");
            return 0;
        }

        if (n_bytes_send < 0) {
            log<ERROR>("TCPSocket::write: Can't write to client\n");
            return -1;
        }

        if (n_bytes_send != static_cast<int>(bytes_send)) {
            log<ERROR>("TCPSocket::write: Some bytes have not been sent\n");
            return -1;
        }

        logf<DEBUG>("[S] [{} bytes]\n", bytes_send);
        return static_cast<int>(bytes_send);
    } else if constexpr (socket_type == WEBSOCK) {
        return websock.send(r);
    } else {
        return -1;
    }
}

// Unix socket session same as TCP session
template<> class Session<UNIX> : public Session<TCP> {
    using Session<TCP>::Session;
};

} // namespace koheron

#endif // __KOHERON_SESSION_HPP__
