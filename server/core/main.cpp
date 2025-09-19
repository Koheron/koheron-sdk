/// Main file for koheron-server
///
/// (c) Koheron

#include "server.hpp"
#include "drivers_executor.hpp"
#include "signal_handler.hpp"

#include "server/core/lib/syslog.hpp"
#include "server/core/lib/services.hpp"
#include "server/core/lib/drivers_manager.hpp"

int main() {
    using namespace koheron;

    auto on_fail = []([[maybe_unused]] driver_id id, [[maybe_unused]] std::string_view name) {
        print<PANIC>("Exiting server...\n");

        if (auto s = services::get<Server>()) {
            s->exit_all = true;
        }
    };

    auto dm = services::provide<DriverManager>(on_fail);

    if (dm->init() < 0) {
        exit(EXIT_FAILURE);
    }

    services::provide<DriverExecutor>();

    auto signal_handler = services::provide<SignalHandler>();

    if (signal_handler->init() < 0) {
        exit(EXIT_FAILURE);
    }

    services::provide<SessionManager>();
    services::provide<Server>()->run();

    exit(EXIT_SUCCESS);
    return 0;
}
