/// Commands of the Server driver
///
/// (c) Koheron

#include "server.hpp"
#include "session.hpp"
#include <ctime>
#include <drivers_json.hpp>

namespace koheron {

// Send the server commit version
#define xstr(s) str(s)
#define str(s) #s

template<> int Server::execute_operation<Server::GET_VERSION>(Command& cmd)
{
    return session_manager.get_session(cmd.session_id).send<1, Server::GET_VERSION>(xstr(KOHERON_VERSION));
}

// Send the commands numbers
template<> int Server::execute_operation<Server::GET_CMDS>(Command& cmd)
{
    return session_manager.get_session(cmd.session_id).send<1, Server::GET_CMDS>(build_drivers_json());
}

////////////////////////////////////////////////

int Server::execute(Command& cmd)
{
    std::lock_guard<std::mutex> lock(this->ks_mutex);

    switch (cmd.operation) {
      case Server::GET_VERSION:
        return execute_operation<Server::GET_VERSION>(cmd);
      case Server::GET_CMDS:
        return execute_operation<Server::GET_CMDS>(cmd);
      case Server::server_op_num:
      default:
        syslog.print<ERROR>("Server::execute unknown operation\n");
        return -1;
    }
}

} // namespace koheron
