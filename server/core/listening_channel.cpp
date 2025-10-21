/// Implementation and specializations of
/// the class ListeningChannel in listening_channel.hpp
///
/// (c) Koheron

#include "server/core/listening_channel.hpp"
#include "server/core/configs/config.hpp"
#include "server/core/socket_session.hpp"
#include "server/core/sockets.hpp"
#include "server/runtime/syslog.hpp"

#include <thread>

namespace koheron {

template<int socket_type>
struct ListenerBackend;

template<unsigned int Port, int ConnectionLimit>
struct TcpListenerBackend {
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
struct ListenerBackend<TCP>
: TcpListenerBackend<config::tcp_port, config::tcp_worker_connections> {};

template<>
struct ListenerBackend<WEBSOCK>
: TcpListenerBackend<config::websocket_port, config::websocket_worker_connections> {};

template<>
struct ListenerBackend<UNIX> {
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
        return open_unix_communication(listen_fd);
    }

    static void close_listener(int listen_fd) {
        close(listen_fd);
    }
};

template<int socket_type>
int ListeningChannel<socket_type>::init() {
    number_of_threads = 0;

    if constexpr (ListenerBackend<socket_type>::enabled()) {
        listen_fd = ListenerBackend<socket_type>::create_listener();
        return listen_fd;
    }

    return 0;
}

template<int socket_type>
void ListeningChannel<socket_type>::shutdown() {
    if constexpr (ListenerBackend<socket_type>::enabled()) {
        logf("Closing {} listener ...\n", listen_channel_desc[socket_type]);

        if (shutdown_communication(listen_fd) < 0) {
            logf<WARNING>("Cannot shutdown socket for {} listener\n", listen_channel_desc[socket_type]);
        }

        ListenerBackend<socket_type>::close_listener(listen_fd);
    }
}

template<int socket_type>
int ListeningChannel<socket_type>::open_communication() {
    return ListenerBackend<socket_type>::open_connection(listen_fd);
}

template<int socket_type>
bool ListeningChannel<socket_type>::is_max_threads() {
    if constexpr (ListenerBackend<socket_type>::enabled()) {
        return number_of_threads >= ListenerBackend<socket_type>::worker_limit();
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
