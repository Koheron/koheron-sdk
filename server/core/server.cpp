/// Implementation of server.hpp
///
/// (c) Koheron

#include "server/core/server.hpp"
#include "server/core/commands.hpp"
#include "server/core/session.hpp"
#include "server/core/session_manager.hpp"
#include "server/core/signal_handler.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/systemd.hpp"

#include <chrono>
#include <cstdlib>

extern "C" {
  #include <sys/un.h>
}

namespace koheron {

Server::Server()
: tcp_listener(this)
, websock_listener(this)
, unix_listener(this)
{
    start_syslog();

    exit_comm.store(false);
    exit_all.store(false);

    if constexpr (config::tcp_worker_connections > 0) {
        if (tcp_listener.init() < 0) {
            exit(EXIT_FAILURE);
        }
    }

    if constexpr (config::websocket_worker_connections > 0) {
        if (websock_listener.init() < 0) {
            exit(EXIT_FAILURE);
        }
    }

    if constexpr (config::unix_socket_worker_connections > 0) {
        if (unix_listener.init() < 0) {
            exit(EXIT_FAILURE);
        }
    }
}

// This cannot be done in the destructor
// since it is called after the "delete config"
// at the end of the main()
void Server::close_listeners()
{
    exit_comm.store(true);
    services::require<SessionManager>().exit_comm();
    tcp_listener.shutdown();
    websock_listener.shutdown();
    unix_listener.shutdown();
    join_listeners_workers();
}

int Server::start_listeners_workers()
{
    if (tcp_listener.start_worker() < 0) {
        return -1;
    }
    if (websock_listener.start_worker() < 0) {
        return -1;
    }
    if (unix_listener.start_worker() < 0) {
        return -1;
    }
    return 0;
}

void Server::join_listeners_workers()
{
    tcp_listener.join_worker();
    websock_listener.join_worker();
    unix_listener.join_worker();
}

bool Server::is_ready()
{
    bool ready = true;

    if constexpr (config::tcp_worker_connections > 0) {
        ready = ready && tcp_listener.is_ready;
    }

    if constexpr (config::websocket_worker_connections > 0) {
        ready = ready && websock_listener.is_ready;
    }

    if constexpr (config::unix_socket_worker_connections > 0) {
        ready = ready && unix_listener.is_ready;
    }

    return ready;
}

int Server::run()
{
    bool ready_notified = false;

    if (start_listeners_workers() < 0) {
        return -1;
    }

    while (true) {
        if (!ready_notified && is_ready()) {
            print<INFO>("Koheron server ready\n");
            if constexpr (config::notify_systemd) {
                systemd::notify_ready("Koheron server is ready");
            }
            ready_notified = true;
        }

        if (services::require<SignalHandler>().interrupt() || exit_all) {
            print<INFO>("Interrupt received, killing Koheron server ...\n");
            services::require<SessionManager>().delete_all();
            close_listeners();
            stop_syslog();
            return 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}

} // namespace koheron
