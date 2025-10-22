/// (c) Koheron

#ifndef __KOHERON_SOCKET_SESSION_HPP__
#define __KOHERON_SOCKET_SESSION_HPP__

#include "server/runtime/syslog.hpp"
#include "server/network/configs/server_definitions.hpp"
#include "server/network/session.hpp"
#include "server/network/websocket.hpp"

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unistd.h>
#include <type_traits>
#include <cassert>
#include <ranges>
#include <span>
#include <cstddef>
#include <cerrno>
#include <sys/socket.h>

namespace net {

class Command;

/// SocketSession
///
/// Receive and parse the client request for execution
template<int socket_type>
class SocketSession : public Session
{
  public:
    SocketSession(int comm_fd, SessionID id_);

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
SocketSession<socket_type>::SocketSession(int comm_fd_, SessionID id_)
: Session(socket_type)
, comm_fd(comm_fd_)
, id(id_)
, websock{}
{
    metadata.set("id", id_);
}

template<int socket_type>
template<std::ranges::contiguous_range R>
int SocketSession<socket_type>::write(const R& r) {
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
template<> class SocketSession<UNIX> : public SocketSession<TCP> {
    using SocketSession<TCP>::SocketSession;
};

} // namespace net

#endif // __KOHERON_SOCKET_SESSION_HPP__
