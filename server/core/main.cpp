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
    auto on_fail = [](koheron::driver_id id, std::string_view name) {
        koheron::print_fmt<PANIC>("DriverManager: driver [{}] {} failed, requesting shutdown.\n", id, name);

        if (auto s = services::get<koheron::Server>()) {
            s->exit_all = true;
        }
    };

    auto dm = services::provide<koheron::DriverManager>(on_fail);

    if (dm->init() < 0) {
        exit(EXIT_FAILURE);
    }

    services::provide<koheron::DriverExecutor>();

    auto signal_handler = services::provide<koheron::SignalHandler>();

    if (signal_handler->init() < 0) {
        exit(EXIT_FAILURE);
    }

    services::provide<koheron::SessionManager>();
    services::provide<koheron::Server>()->run();

    exit(EXIT_SUCCESS);
    return 0;
}
