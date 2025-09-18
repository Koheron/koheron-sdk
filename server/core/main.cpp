/// Main file for koheron-server
///
/// (c) Koheron

#include "server_definitions.hpp"
#include "config.hpp"
#include "server.hpp"
#include "services.hpp"
#include "drivers_manager.hpp"

int main() {
    koheron::Server server;
    auto dm = services::provide<koheron::DriverManager>(&server);

    if (dm->init() < 0) {
        exit(EXIT_FAILURE);
    }

    server.run();
    exit(EXIT_SUCCESS);
    return 0;
}
