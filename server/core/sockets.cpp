
#include "server/core/sockets.hpp"
#include "server/core/configs/config.hpp"
#include "server/core/configs/server_definitions.hpp"
#include "server/runtime/syslog.hpp"

#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/un.h>

namespace koheron {

// -----------------------------------------------
// TCP
// -----------------------------------------------

int create_tcp_listening_socket(unsigned int port) {
    int listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd_ < 0) {
        log<PANIC>("Can't open socket\n");
        return -1;
    }

    // To avoid binding error
    // See http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#bind
    int yes = 1;

    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
                  &yes, sizeof(int))==-1) {
        log<CRITICAL>("Cannot set SO_REUSEADDR\n");
    }

    if constexpr (config::tcp_nodelay) {
        int one = 1;

        if (::setsockopt(listen_fd_, IPPROTO_TCP, TCP_NODELAY,
                         &one, sizeof(one)) < 0) {
            // This is only considered critical since it is performance
            // related but this doesn't prevent to use the socket
            // so only log the error and keep going ...
            log<CRITICAL>("Cannot set TCP_NODELAY\n");
        }
    }

    struct sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);

    // Assign name (address) to socket
    if (::bind(listen_fd_, reinterpret_cast<struct sockaddr *>(&servaddr),
             sizeof(servaddr)) < 0) {
        log<PANIC>("Binding error\n");
        ::close(listen_fd_);
        return -1;
    }

    return listen_fd_;
}

int set_socket_options(int comm_fd) {
    int sndbuf_len = sizeof(uint32_t) * KOHERON_SIG_LEN;

    if (::setsockopt(comm_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf_len, sizeof(sndbuf_len)) < 0) {
        log<CRITICAL>("Cannot set socket send options\n");
        ::close(comm_fd);
        return -1;
    }

    int rcvbuf_len = KOHERON_READ_STR_LEN;

    if (::setsockopt(comm_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_len, sizeof(rcvbuf_len)) < 0) {
        log<CRITICAL>("Cannot set socket receive options\n");
        ::close(comm_fd);
        return -1;
    }

    if constexpr (config::tcp_nodelay) {
        int one = 1;

        if (::setsockopt(comm_fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&one), sizeof(one)) < 0) {
            log<CRITICAL>("Cannot set TCP_NODELAY\n");
            ::close(comm_fd);
            return -1;
        }
    }

    return 0;
}

int open_tcp_communication(int listen_fd) {
    int comm_fd = ::accept(listen_fd, nullptr, nullptr);

    if (comm_fd < 0) {
        return comm_fd;
    }

    if (set_socket_options(comm_fd) < 0) {
        return -1;
    }

    return comm_fd;
}

// -----------------------------------------------
// UNIX
// -----------------------------------------------

int create_unix_listening_socket(const char *unix_sock_path) {
    struct sockaddr_un local{};

    int listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);

    if (listen_fd_ < 0) {
        log<PANIC>("Can't open Unix socket\n");
        return -1;
    }

    local.sun_family = AF_UNIX;

    if (std::snprintf(local.sun_path, sizeof(local.sun_path), "%s", unix_sock_path)
            >= static_cast<int>(sizeof(local.sun_path))) {
        log<PANIC>("Unix socket path too long\n");
        ::close(listen_fd_);
        return -1;
    }

    ::unlink(local.sun_path);
    auto len = strlen(local.sun_path) + sizeof(local.sun_family);

    if (::bind(listen_fd_, reinterpret_cast<struct sockaddr *>(&local), len) < 0) {
        log<PANIC>("Unix socket binding error\n");
        ::close(listen_fd_);
        return -1;
    }

    return listen_fd_;
}

int open_unix_communication(int listen_fd) {
    struct sockaddr_un remote{};
    uint32_t t = sizeof(remote);
    return ::accept(listen_fd, reinterpret_cast<struct sockaddr *>(&remote), &t);
}

// -----------------------------------------------
// ANY
// -----------------------------------------------

int shutdown_communication(int listen_fd) {
    return ::shutdown(listen_fd, SHUT_RDWR);
}

} // namespace koheron