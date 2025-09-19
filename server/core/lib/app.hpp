#include "syslog.hpp"
#include "services.hpp"
#include "drivers_manager.hpp"

namespace koheron {

auto& start_app() {
    start_syslog();

    auto on_fail = [](driver_id id, std::string_view name) {
        print_fmt<PANIC>("DriverManager: driver [{}] {} failed to allocate.\n", id, name);
        exit(EXIT_FAILURE);
    };

    auto dm = services::provide<DriverManager>(on_fail);

    if (dm->init() < 0) {
        exit(EXIT_FAILURE);
    }

    return dm->context();
}

void stop_app() {
    stop_syslog();
}

} // namespace koheron