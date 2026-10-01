/// ListenerManager implementation
///
/// (c) Koheron

#include "server/network/listener_manager.hpp"

#include <cstdlib>
#include <sys/socket.h>

namespace net {

ListenerManager::ListenerManager()
: exit_comm_(false)
{
    services::provide<SessionManager>();

    if (tcp_listener_.init() < 0) {
        std::exit(EXIT_FAILURE);
    }

    if (websock_listener_.init() < 0) {
        std::exit(EXIT_FAILURE);
    }

    if (unix_listener_.init() < 0) {
        std::exit(EXIT_FAILURE);
    }
}

int ListenerManager::start() {
    if (start_listener(tcp_listener_) < 0) {
        return -1;
    }

    if (start_listener(websock_listener_) < 0) {
        return -1;
    }

    if (start_listener(unix_listener_) < 0) {
        return -1;
    }

    return 0;
}

void ListenerManager::request_stop() {
    exit_comm_.store(true, std::memory_order_release);
}

void ListenerManager::shutdown() {
    request_stop();
    services::require<SessionManager>().exit_comm();

    tcp_listener_.shutdown();
    websock_listener_.shutdown();
    unix_listener_.shutdown();

    tcp_listener_.join_worker();
    websock_listener_.join_worker();
    unix_listener_.join_worker();

    services::require<SessionManager>().delete_all();
}

bool ListenerManager::is_ready() const {
    bool ready = true;

    if constexpr (config::tcp_worker_connections > 0) {
        ready = ready && tcp_listener_.is_ready;
    }

    if constexpr (config::websocket_worker_connections > 0) {
        ready = ready && websock_listener_.is_ready;
    }

    if constexpr (config::unix_socket_worker_connections > 0) {
        ready = ready && unix_listener_.is_ready;
    }

    return ready;
}

bool ListenerManager::should_stop() const {
    return exit_comm_.load(std::memory_order_acquire);
}

template<int socket_type>
int ListenerManager::start_listener(ListeningChannel<socket_type>& listener) {
    if (listener.listen_fd >= 0) {
        if (::listen(listener.listen_fd, NUMBER_OF_PENDING_CONNECTIONS) < 0) {
            logf<PANIC>("Listen {} error\n", listen_channel_desc[socket_type]);
            return -1;
        }

        listener.listening_thread = std::thread{listening_thread_call<socket_type>, &listener, this};
    }

    return 0;
}

// Explicit instantiations for supported socket types

template int ListenerManager::start_listener<TCP>(ListeningChannel<TCP>& listener);
template int ListenerManager::start_listener<WEBSOCK>(ListeningChannel<WEBSOCK>& listener);
template int ListenerManager::start_listener<UNIX>(ListeningChannel<UNIX>& listener);

} // namespace net
