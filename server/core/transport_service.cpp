/// TransportService implementation
///
/// (c) Koheron

#include "server/core/transport_service.hpp"

#include <cstdlib>

namespace koheron {

TransportService::TransportService()
: exit_comm_(false)
{
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

int TransportService::start() {
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

void TransportService::request_stop() {
    exit_comm_.store(true, std::memory_order_release);
}

void TransportService::shutdown() {
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

bool TransportService::is_ready() const {
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

bool TransportService::should_stop() const {
    return exit_comm_.load(std::memory_order_acquire);
}

template<int socket_type>
int TransportService::start_listener(ListeningChannel<socket_type>& listener) {
    return listener.start_worker(*this);
}

// Explicit instantiations for supported socket types

template int TransportService::start_listener<TCP>(ListeningChannel<TCP>& listener);
template int TransportService::start_listener<WEBSOCK>(ListeningChannel<WEBSOCK>& listener);
template int TransportService::start_listener<UNIX>(ListeningChannel<UNIX>& listener);

} // namespace koheron
