/// Server main class
///
/// (c) Koheron

#ifndef __KOHERON_HPP__
#define __KOHERON_HPP__

#include "server_definitions.hpp"
#include "config.hpp"

#include <thread>
#include <mutex>
#include <array>
#include <vector>
#include <string>
#include <atomic>
#include <ctime>
#include <utility>

#include "drivers_manager.hpp"
#include "syslog.hpp"
#include "signal_handler.hpp"
#include "session_manager.hpp"

namespace koheron {

template<int socket_type> class Session;

////////////////////////////////////////////////////////////////////////////
/////// ListeningChannel

template<int socket_type>
struct ListenerStats
{
    int number_of_opened_sessions = 0; ///< Number of currently opened sessions
    int total_sessions_num = 0;  ///< Total number of sessions
    int total_number_of_requests = 0;  ///< Total number of requests
};

/// Implementation in listening_channel.cpp
template<int socket_type>
class ListeningChannel
{
  public:
    ListeningChannel(Server *server_)
    : listen_fd(-1)
    , is_ready(false)
    , server(server_)
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

    /// Listening socket ID
    int listen_fd;

    /// Number of sessions using the channel
    std::atomic<int> number_of_threads;

    /// True when ready to open sessions
    std::atomic<bool> is_ready;

    /// Listening thread
    std::thread comm_thread;

    Server *server;
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

/// Main class of the server. It initializes the
/// connections and start the sessions.

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

    SignalHandler signal_handler;

    std::atomic<bool> exit_comm;
    std::atomic<bool> exit_all;

    // Listeners
    ListeningChannel<TCP> tcp_listener;
    ListeningChannel<WEBSOCK> websock_listener;
    ListeningChannel<UNIX> unix_listener;

    /// True when all listeners are ready
    bool is_ready();

    // Managers
    DriverManager driver_manager;
    SysLog syslog;
    SessionManager session_manager;

    std::mutex ks_mutex;

    int execute(Command& cmd);
    template<int op> int execute_operation(Command& cmd);

  private:
    // Internal functions
    int start_listeners_workers();
    void detach_listeners_workers();
    void join_listeners_workers();
    void close_listeners();
    void notify_systemd_ready();

template<int socket_type> friend class ListeningChannel;
};

template<int socket_type>
void session_thread_call(int comm_fd, ListeningChannel<socket_type> *listener)
{
    listener->number_of_threads++;
    listener->stats.number_of_opened_sessions++;
    listener->stats.total_sessions_num++;

    SessionID sid = listener->server->session_manager. template create_session<socket_type>(comm_fd);

    auto session = static_cast<Session<socket_type>*>(&listener->server->session_manager.get_session(sid));

    if (session->run() < 0) {
        listener->server->syslog. template print<ERROR>("An error occured during session\n");
    }

    listener->server->session_manager.delete_session(sid);
    listener->number_of_threads--;
    listener->stats.number_of_opened_sessions--;
}

template<int socket_type>
void comm_thread_call(ListeningChannel<socket_type> *listener)
{
    listener->is_ready = true;

    while (!listener->server->exit_comm.load()) {
        int comm_fd = listener->open_communication();

        if (listener->server->exit_comm.load())
            break;

        if (comm_fd < 0) {
            listener->server->syslog. template print<CRITICAL>("Connection to client rejected [socket_type = %u]\n", socket_type);
            continue;
        }

        if (listener->is_max_threads()) {
            listener->server->syslog. template print<WARNING>("Maximum number of workers exceeded\n");
            continue;
        }

        std::thread session_thread(session_thread_call<socket_type>, comm_fd, listener);
        session_thread.detach();
    }

    listener->server->syslog. template print<INFO>("%s listener closed.\n", listen_channel_desc[socket_type].c_str());
}

template<int socket_type>
inline int ListeningChannel<socket_type>::start_worker()
{
    if (listen_fd >= 0) {
        if (listen(listen_fd, NUMBER_OF_PENDING_CONNECTIONS) < 0) {
            server->syslog.print<PANIC>("Listen %s error\n", listen_channel_desc[socket_type].c_str());
            return -1;
        }
        comm_thread = std::thread{comm_thread_call<socket_type>, this};
    }

    return 0;
}

} // namespace koheron

#endif // __KOHERON_HPP__
