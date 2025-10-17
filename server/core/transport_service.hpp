/// Transport layer service for listener orchestration
///
/// (c) Koheron

#ifndef __SERVER_CORE_TRANSPORT_SERVICE_HPP__
#define __SERVER_CORE_TRANSPORT_SERVICE_HPP__

#include "server/core/configs/server_definitions.hpp"
#include "server/core/configs/config.hpp"
#include "server/core/session_manager.hpp"
#include "server/core/session.hpp"

#include "server/runtime/services.hpp"
#include "server/runtime/syslog.hpp"

#include <atomic>
#include <thread>
#include <utility>
#include <sys/types.h>
#include <sys/socket.h>

namespace koheron {

class TransportService;

template<int socket_type>
struct ListenerStats {
    int number_of_opened_sessions = 0; ///< Number of currently opened sessions
    int total_sessions_num = 0;        ///< Total number of sessions
    int total_number_of_requests = 0;  ///< Total number of requests
};

template<int socket_type>
class ListeningChannel {
  public:
    ListeningChannel()
    : listen_fd(-1)
    , number_of_threads(-1)
    , is_ready(false)
    {}

    int init();
    void shutdown();

    /// True if the maximum of threads set by the config is reached
    bool is_max_threads();

    int start_worker(TransportService& transport);
    void join_worker();
    int open_communication();

    int listen_fd;
    std::atomic<int> number_of_threads; // Number of sessions using the channel
    std::atomic<bool> is_ready;
    std::thread comm_thread; // Listening thread
    ListenerStats<socket_type> stats;
};

template<int socket_type>
void session_thread_call(int comm_fd, ListeningChannel<socket_type>* listener);

template<int socket_type>
void comm_thread_call(ListeningChannel<socket_type>* listener, TransportService* transport);

class TransportService {
  public:
    TransportService();

    int start();
    void request_stop();
    void shutdown();

    bool is_ready() const;
    bool should_stop() const;

  private:
    template<int socket_type>
    int start_listener(ListeningChannel<socket_type>& listener);

    std::atomic<bool> exit_comm_;
    ListeningChannel<TCP> tcp_listener_;
    ListeningChannel<WEBSOCK> websock_listener_;
    ListeningChannel<UNIX> unix_listener_;

    template<int socket_type>
    friend void comm_thread_call(ListeningChannel<socket_type>* listener, TransportService* transport);
};

// -----------------------------------------------------------------------------
// Template implementations
// -----------------------------------------------------------------------------

template<int socket_type>
void ListeningChannel<socket_type>::join_worker() {
    if (listen_fd >= 0 && comm_thread.joinable()) {
        comm_thread.join();
    }
}

template<int socket_type>
int ListeningChannel<socket_type>::start_worker(TransportService& transport) {
    if (listen_fd >= 0) {
        if (::listen(listen_fd, NUMBER_OF_PENDING_CONNECTIONS) < 0) {
            logf<PANIC>("Listen {} error\n", listen_channel_desc[socket_type]);
            return -1;
        }
        comm_thread = std::thread{comm_thread_call<socket_type>, this, &transport};
    }

    return 0;
}

template<int socket_type>
void session_thread_call(int comm_fd, ListeningChannel<socket_type>* listener) {
    listener->number_of_threads++;
    listener->stats.number_of_opened_sessions++;
    listener->stats.total_sessions_num++;

    auto& sm = services::require<SessionManager>();
    auto sid = sm.template create_session<socket_type>(comm_fd);
    auto session = static_cast<Session<socket_type>*>(&sm.get_session(sid));

    if (session->run() < 0) {
        log<ERROR>("An error occured during session\n");
    }

    sm.delete_session(sid);
    listener->number_of_threads--;
    listener->stats.number_of_opened_sessions--;
}

template<int socket_type>
void comm_thread_call(ListeningChannel<socket_type>* listener, TransportService* transport) {
    listener->is_ready = true;

    while (!transport->should_stop()) {
        int comm_fd = listener->open_communication();

        if (transport->should_stop()) {
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

} // namespace koheron

#endif // __SERVER_CORE_TRANSPORT_SERVICE_HPP__
