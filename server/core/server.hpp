/// Server main class
///
/// (c) Koheron

#ifndef __KOHERON_HPP__
#define __KOHERON_HPP__

#include <thread>
#include <mutex>
#include <array>
#include <vector>
#include <string>
#include <atomic>
#include <ctime>
#include <utility>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

#include "server/core/configs/server_definitions.hpp"
#include "server/core/configs/config.hpp"
#include "server/core/session_manager.hpp"

#include "server/runtime/services.hpp"
#include "server/runtime/syslog.hpp"

namespace koheron {

////////////////////////////////////////////////////////////////////////////
/////// ListeningChannel

template<int socket_type>
struct ListenerStats
{
    int number_of_opened_sessions = 0; ///< Number of currently opened sessions
    int total_sessions_num = 0;        ///< Total number of sessions
    int total_number_of_requests = 0;  ///< Total number of requests
};

/// Implementation in listening_channel.cpp
template<int socket_type>
class ListeningChannel
{
  public:
    ListeningChannel()
    : listen_fd(-1)
    , is_ready(false)
    {
        number_of_threads.store(-1);
    }

    int init();
    void shutdown();

    /// True if the maximum of threads set by the config is reached
    bool is_max_threads();

    int start_worker();
    void join_worker();
    int open_communication();

    int listen_fd;
    std::atomic<int> number_of_threads; // Number of sessions using the channel
    std::atomic<bool> is_ready;
    std::thread comm_thread; // Listening thread
    ListenerStats<socket_type> stats;

}; // ListeningChannel

template<int socket_type>
void ListeningChannel<socket_type>::join_worker()
{
    if (listen_fd >= 0) {
        comm_thread.join();
    }
}

////////////////////////////////////////////////////////////////////////////
/////// Server

class Server
{
  public:
    Server();

    int run();

    /// Operations associated to the driver
    enum Operation {
        GET_VERSION = 0,            ///< Send th version of the server
        GET_CMDS = 1,               ///< Send the commands numbers
        server_op_num
    };

    std::atomic<bool> exit_comm;
    std::atomic<bool> exit_all;

    ListeningChannel<TCP>     tcp_listener;
    ListeningChannel<WEBSOCK> websock_listener;
    ListeningChannel<UNIX>    unix_listener;

    bool is_ready(); // True when all listeners are ready

    std::mutex ks_mutex;

    int execute(Command& cmd);
    template<int op> int execute_operation(Command& cmd);

  private:
    int start_listeners_workers();
    void detach_listeners_workers();
    void join_listeners_workers();
    void close_listeners();
};

template<int socket_type>
void session_thread_call(int comm_fd, ListeningChannel<socket_type> *listener)
{
    listener->number_of_threads++;
    listener->stats.number_of_opened_sessions++;
    listener->stats.total_sessions_num++;

    auto& sm = services::require<SessionManager>();
    auto sid = sm. template create_session<socket_type>(comm_fd);
    auto session = static_cast<Session<socket_type>*>(&sm.get_session(sid));

    if (session->run() < 0) {
        rt::print<ERROR>("An error occured during session\n");
    }

    sm.delete_session(sid);
    listener->number_of_threads--;
    listener->stats.number_of_opened_sessions--;
}

template<int socket_type>
void comm_thread_call(ListeningChannel<socket_type> *listener)
{
    listener->is_ready = true;
    auto& serv = services::require<Server>();

    while (!serv.exit_comm.load()) {
        int comm_fd = listener->open_communication();

        if (serv.exit_comm.load()) {
            break;
        }

        if (comm_fd < 0) {
            log<CRITICAL>("Connection to client rejected [socket_type = %u]\n", socket_type);
            continue;
        }

        if (listener->is_max_threads()) {
            log<WARNING>("Maximum number of workers exceeded\n");
            continue;
        }

        std::thread session_thread(session_thread_call<socket_type>, comm_fd, listener);
        session_thread.detach();
    }

    logf("{} listener closed.\n", listen_channel_desc[socket_type]);
}

template<int socket_type>
int ListeningChannel<socket_type>::start_worker()
{
    if (listen_fd >= 0) {
        if (::listen(listen_fd, NUMBER_OF_PENDING_CONNECTIONS) < 0) {
            logf<PANIC>("Listen {} error\n", listen_channel_desc[socket_type]);
            return -1;
        }
        comm_thread = std::thread{comm_thread_call<socket_type>, this};
    }

    return 0;
}

} // namespace koheron

#endif // __KOHERON_HPP__
