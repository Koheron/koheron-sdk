/// Main file for koheron-server
///
/// (c) Koheron

#include "server_definitions.hpp"
#include "config.hpp"
#include "server.hpp"

int main() {
    koheron::Server server;
    server.run();
    exit(EXIT_SUCCESS);
    return 0;
}
