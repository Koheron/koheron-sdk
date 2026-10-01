#include "server/runtime/executor.hpp"
#include "server/network/commands.hpp"
#include "server/runtime/syslog.hpp"

namespace rt {

int IExecutor::execute(net::Command& cmd) {
    if (cmd.driver == 0) {  // NoDriver
        return 0;
    }

    if (cmd.driver == 1) { // KServer
        switch (cmd.operation) {
          case GET_VERSION:
            return cmd.send(server_version());
          case GET_CMDS:
            return cmd.send(drivers_json());
          default:
            log<ERROR>("Server::execute unknown operation\n");
            return -1;
        }
    }

    return handle_app(cmd);
}

} // namespace rt