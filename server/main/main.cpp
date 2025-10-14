/// Main file for koheron-server
///
/// (c) Koheron

#include "server/core/server.hpp"
#include "server/core/drivers_executor.hpp"

#include "server/runtime/signal_handler.hpp"
#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/drivers_manager.hpp"

int main() {
    using namespace koheron;

    // On driver allocation failure
    auto on_fail = []([[maybe_unused]] driver_id id, [[maybe_unused]] std::string_view name) {
        rt::print<PANIC>("Exiting server...\n");
        services::get<Server>()->exit_all = true;
    };

    if (services::provide<rt::DriverManager>(on_fail)->init() < 0) {
        exit(EXIT_FAILURE);
    }

    services::provide<DriverExecutor>();

    if (services::provide<rt::SignalHandler>()->init() < 0) {
        exit(EXIT_FAILURE);
    }

    services::provide<SessionManager>();
    services::provide<Server>()->run();

    exit(EXIT_SUCCESS);
    return 0;
}
