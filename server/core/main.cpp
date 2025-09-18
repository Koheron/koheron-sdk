/// Main file for koheron-server
///
/// (c) Koheron

#include "server_definitions.hpp"
#include "config.hpp"
#include "server.hpp"
#include "services.hpp"
#include "drivers_manager.hpp"

int main() {
    auto dm = services::provide<koheron::DriverManager>();

    if (dm->init() < 0) {
        exit(EXIT_FAILURE);
    }

    auto server = services::provide<koheron::Server>();
    server->run();

    exit(EXIT_SUCCESS);
    return 0;
}
