/// Implementation and specializations of
/// the class ListeningChannel in listening_channel.hpp
///
/// (c) Koheron

#include "server/core/listening_channel.hpp"
#include "server/core/configs/config.hpp"
#include "server/core/session.hpp"
#include "server/runtime/syslog.hpp"

#include <thread>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/un.h>

namespace koheron {

static int create_tcp_listening_socket(unsigned int port) {
    int listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd_ < 0) {
        log<PANIC>("Can't open socket\n");
        return -1;
    }

    // To avoid binding error
    // See http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#bind
    int yes = 1;

    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
                  &yes, sizeof(int))==-1) {
        log<CRITICAL>("Cannot set SO_REUSEADDR\n");
    }

    if constexpr (config::tcp_nodelay) {
        int one = 1;

        if (setsockopt(listen_fd_, IPPROTO_TCP, TCP_NODELAY,
                       &one, sizeof(one)) < 0) {
            // This is only considered critical since it is performance
            // related but this doesn't prevent to use the socket
            // so only log the error and keep going ...
            log<CRITICAL>("Cannot set TCP_NODELAY\n");
        }
    }

    struct sockaddr_in servaddr{};
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);

    // Assign name (address) to socket
    if (bind(listen_fd_, reinterpret_cast<struct sockaddr *>(&servaddr),
             sizeof(servaddr)) < 0) {
        log<PANIC>("Binding error\n");
        close(listen_fd_);
        return -1;
    }

    return listen_fd_;
}

static int set_socket_options(int comm_fd) {
    int sndbuf_len = sizeof(uint32_t) * KOHERON_SIG_LEN;

    if (setsockopt(comm_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf_len, sizeof(sndbuf_len)) < 0) {
        log<CRITICAL>("Cannot set socket send options\n");
        close(comm_fd);
        return -1;
    }

    int rcvbuf_len = KOHERON_READ_STR_LEN;

    if (setsockopt(comm_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_len, sizeof(rcvbuf_len)) < 0) {
        log<CRITICAL>("Cannot set socket receive options\n");
        close(comm_fd);
        return -1;
    }

    if constexpr (config::tcp_nodelay) {
        int one = 1;

        if (setsockopt(comm_fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&one), sizeof(one)) < 0) {
            log<CRITICAL>("Cannot set TCP_NODELAY\n");
            close(comm_fd);
            return -1;
        }
    }

    return 0;
}

static int open_tcp_communication(int listen_fd) {
    int comm_fd = accept(listen_fd, nullptr, nullptr);

    if (comm_fd < 0) {
        return comm_fd;
    }

    if (set_socket_options(comm_fd) < 0) {
        return -1;
    }

    return comm_fd;
}

static int create_unix_listening_socket(const char *unix_sock_path) {
    struct sockaddr_un local{};

    int listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);

    if (listen_fd_ < 0) {
        log<PANIC>("Can't open Unix socket\n");
        return -1;
    }

    memset(&local, 0, sizeof(struct sockaddr_un));
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, unix_sock_path);
    unlink(local.sun_path);
    auto len = strlen(local.sun_path) + sizeof(local.sun_family);

    if (bind(listen_fd_, reinterpret_cast<struct sockaddr *>(&local), len) < 0) {
        log<PANIC>("Unix socket binding error\n");
        close(listen_fd_);
        return -1;
    }

    return listen_fd_;
}

// ---- Transport policies ------------------------------------------------------

template<int socket_type>
struct ListeningChannelTransport;

template<unsigned int Port, int ConnectionLimit>
struct TcpLikeListeningTransport {
    static constexpr bool enabled() {
        return ConnectionLimit > 0;
    }

    static constexpr int worker_limit() {
        return ConnectionLimit;
    }

    static int create_listener() {
        return create_tcp_listening_socket(Port);
    }

    static int open_connection(int listen_fd) {
        return open_tcp_communication(listen_fd);
    }

    static void close_listener(int listen_fd) {
        close(listen_fd);
    }
};

template<>
struct ListeningChannelTransport<TCP>
: TcpLikeListeningTransport<config::tcp_port, config::tcp_worker_connections> {};

template<>
struct ListeningChannelTransport<WEBSOCK>
: TcpLikeListeningTransport<config::websocket_port, config::websocket_worker_connections> {};

template<>
struct ListeningChannelTransport<UNIX> {
    static constexpr bool enabled() {
        return config::unix_socket_worker_connections > 0;
    }

    static constexpr int worker_limit() {
        return config::unix_socket_worker_connections;
    }

    static int create_listener() {
        return create_unix_listening_socket(config::unix_socket_path);
    }

    static int open_connection(int listen_fd) {
        struct sockaddr_un remote{};
        uint32_t t = sizeof(remote);
        return accept(listen_fd, reinterpret_cast<struct sockaddr *>(&remote), &t);
    }

    static void close_listener(int listen_fd) {
        close(listen_fd);
    }
};

// ---- Generic ListeningChannel implementation --------------------------------

template<int socket_type>
int ListeningChannel<socket_type>::init() {
    number_of_threads = 0;

    if constexpr (ListeningChannelTransport<socket_type>::enabled()) {
        listen_fd = ListeningChannelTransport<socket_type>::create_listener();
        return listen_fd;
    }

    return 0;
}

template<int socket_type>
void ListeningChannel<socket_type>::shutdown() {
    if constexpr (ListeningChannelTransport<socket_type>::enabled()) {
        logf("Closing {} listener ...\n", listen_channel_desc[socket_type]);

        if (::shutdown(listen_fd, SHUT_RDWR) < 0) {
            logf<WARNING>("Cannot shutdown socket for {} listener\n", listen_channel_desc[socket_type]);
        }

        ListeningChannelTransport<socket_type>::close_listener(listen_fd);
    }
}

template<int socket_type>
int ListeningChannel<socket_type>::open_communication() {
    return ListeningChannelTransport<socket_type>::open_connection(listen_fd);
}

template<int socket_type>
bool ListeningChannel<socket_type>::is_max_threads() {
    if constexpr (ListeningChannelTransport<socket_type>::enabled()) {
        return number_of_threads >= ListeningChannelTransport<socket_type>::worker_limit();
    }

    return false;
}

// Explicit instantiations for supported socket types
template int ListeningChannel<TCP>::init();
template void ListeningChannel<TCP>::shutdown();
template int ListeningChannel<TCP>::open_communication();
template bool ListeningChannel<TCP>::is_max_threads();

template int ListeningChannel<WEBSOCK>::init();
template void ListeningChannel<WEBSOCK>::shutdown();
template int ListeningChannel<WEBSOCK>::open_communication();
template bool ListeningChannel<WEBSOCK>::is_max_threads();

template int ListeningChannel<UNIX>::init();
template void ListeningChannel<UNIX>::shutdown();
template int ListeningChannel<UNIX>::open_communication();
template bool ListeningChannel<UNIX>::is_max_threads();

} // namespace koheron
