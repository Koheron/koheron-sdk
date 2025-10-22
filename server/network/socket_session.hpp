/// (c) Koheron

#ifndef __KOHERON_SOCKET_SESSION_HPP__
#define __KOHERON_SOCKET_SESSION_HPP__

#include "server/runtime/syslog.hpp"
#include "server/network/configs/server_definitions.hpp"
#include "server/network/session.hpp"
#include "server/network/websocket.hpp"
#include "server/network/commands.hpp"

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
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netdb.h>

namespace net {

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

    void set_socket_infos();
};

template<int socket_type>
SocketSession<socket_type>::SocketSession(int comm_fd_, SessionID id_)
: Session(socket_type)
, comm_fd(comm_fd_)
, id(id_)
, websock{}
{
    set_socket_infos();
}

template<int socket_type>
int SocketSession<socket_type>::init_socket() {
    if constexpr (socket_type == WEBSOCK) {
        websock.set_id(comm_fd);

        if (websock.authenticate() < 0) {
            log<CRITICAL>("Cannot connect websocket to client\n");
            return -1;
        }
    }

    return 0; // TCP/UNIX do nothing
}

template<int socket_type>
int SocketSession<socket_type>::exit_socket() {
    if constexpr (socket_type == WEBSOCK) {
        return websock.exit();
    } else {
        return 0; // TCP/UNIX do nothing
    }
}

template<int socket_type>
int SocketSession<socket_type>::read_command(Command& cmd) {
    if constexpr (socket_type == WEBSOCK) {
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

        cmd.socket_type = WEBSOCK;
    } else { // TCP/UNIX
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

        cmd.socket_type = TCP;
    }

    const auto [drv_id, op] = cmd.header.deserialize<uint16_t, uint16_t>();
    cmd.session_id = id;
    cmd.session = this;
    cmd.comm_fd = comm_fd;
    cmd.driver = drv_id;
    cmd.operation = op;

    logf<DEBUG>("WebSocket: Receive command for driver {}, operation {}\n",
            cmd.driver, cmd.operation);

    return Command::HEADER_SIZE;
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

template<int socket_type>
void SocketSession<socket_type>::set_socket_infos() {
    infos.set("id", id);

    if constexpr (socket_type == TCP || socket_type == WEBSOCK) {
        // family + peer/local addresses
        sockaddr_storage ss_peer{}, ss_local{};
        socklen_t spl = sizeof(ss_peer), sll = sizeof(ss_local);

        if (::getpeername(comm_fd, reinterpret_cast<sockaddr*>(&ss_peer), &spl) == 0) {
            char host[NI_MAXHOST]{}, serv[NI_MAXSERV]{};

            if (::getnameinfo(reinterpret_cast<sockaddr*>(&ss_peer), spl,
                              host, sizeof(host), serv, sizeof(serv),
                              NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
                infos.set("peer_ip", std::string_view{host});
                infos.set("peer_port", std::strtol(serv, nullptr, 10));
            }

            infos.set("family",
                ss_peer.ss_family == AF_INET ? "AF_INET" :
                ss_peer.ss_family == AF_INET6 ? "AF_INET6" : "AF_OTHER");
        }

        if (::getsockname(comm_fd, reinterpret_cast<sockaddr*>(&ss_local), &sll) == 0) {
            char host[NI_MAXHOST]{}, serv[NI_MAXSERV]{};

            if (::getnameinfo(reinterpret_cast<sockaddr*>(&ss_local), sll,
                              host, sizeof(host), serv, sizeof(serv),
                              NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
                infos.set("local_ip", std::string_view{host});
                infos.set("local_port", std::strtol(serv, nullptr, 10));
            }
        }
    } else { // socket_type == UNIX
        ucred uc{}; socklen_t ucl = sizeof(uc);

        if (::getsockopt(comm_fd, SOL_SOCKET, SO_PEERCRED, &uc, &ucl) == 0) {
            infos.set("peer_uid", static_cast<std::int64_t>(uc.uid));
            infos.set("peer_gid", static_cast<std::int64_t>(uc.gid));
            infos.set("peer_pid", static_cast<std::int64_t>(uc.pid));
        }

        infos.set("family", "AF_UNIX");
    }
}

} // namespace net

#endif // __KOHERON_SOCKET_SESSION_HPP__
