/// Implementation of server.hpp
///
/// (c) Koheron

#include "server/core/server.hpp"
#include "server/core/session_manager.hpp"
#include "server/core/transport_service.hpp"

#include "server/runtime/signal_handler.hpp"
#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/systemd.hpp"

#include <chrono>

namespace koheron {

Server::Server() {
    exit_all.store(false);
}

int Server::run() {
    bool ready_notified = false;
    auto& transport = services::require<TransportService>();

    if (transport.start() < 0) {
        return -1;
    }

    while (true) {
        if (!ready_notified && transport.is_ready()) {
            log("Koheron server ready\n");

            if constexpr (config::notify_systemd) {
                rt::systemd::notify_ready("Koheron server is ready");
            }

            ready_notified = true;
        }

        if (services::require<rt::SignalHandler>().interrupt() || exit_all) {
            log("Interrupt received, killing Koheron server ...\n");
            services::require<SessionManager>().delete_all();
            transport.shutdown();
            transport.join();
            return 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}

} // namespace koheron
