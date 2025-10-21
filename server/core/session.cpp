#include "server/runtime/services.hpp"
#include "server/runtime/syslog.hpp"
#include "server/core/commands.hpp"
#include "server/core/session.hpp"
#include "server/executor/driver_executor.hpp"

namespace koheron {

int Session::run() {
    if (init_socket() < 0) {
        return -1;
    }

    while (!exit_signal) {
        Command cmd;
        const int nb_bytes_rcvd = read_command(cmd);

        if (exit_signal) {
            break;
        }

        if (nb_bytes_rcvd <= 0) {
            // We don't call exit_session() here because the socket is already closed.
            return nb_bytes_rcvd;
        }

        if (services::require<DriverExecutor>().execute(cmd) < 0) {
            logf<ERROR>("Failed to execute command [driver = {}, operation = {}]\n",
                        cmd.driver, cmd.operation);
        }

        if (status == CLOSED) {
            break;
        }
    }

    if (exit_socket() < 0) {
        log<WARNING>("An error occured during session exit\n");
    }

    return 0;
}

} // namespace koheron
