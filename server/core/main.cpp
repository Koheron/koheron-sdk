/// Main file for koheron-server
///
/// (c) Koheron

#include "server_definitions.hpp"
#include "config.hpp"
#include "server.hpp"
#include "services.hpp"
#include "drivers_manager.hpp"
#include "drivers_executor.hpp"
#include "signal_handler.hpp"

int main() {
    auto dm = services::provide<koheron::DriverManager>();

    if (dm->init() < 0) {
        exit(EXIT_FAILURE);
    }

    services::provide<koheron::DriverExecutor>();

    auto signal_handler = services::provide<koheron::SignalHandler>();

    if (signal_handler->init() < 0) {
        exit(EXIT_FAILURE);
    }

    services::provide<koheron::SessionManager>();

    auto server = services::provide<koheron::Server>();
    server->run();

    exit(EXIT_SUCCESS);
    return 0;
}
