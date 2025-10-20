/// Listeners orchestration
///
/// (c) Koheron

#ifndef __SERVER_CORE_LISTENER_MANAGER_HPP__
#define __SERVER_CORE_LISTENER_MANAGER_HPP__

#include "server/core/configs/server_definitions.hpp"
#include "server/core/configs/config.hpp"
#include "server/core/listening_channel.hpp"
#include "server/core/session_manager.hpp"
#include "server/core/session.hpp"

#include "server/runtime/services.hpp"
#include "server/runtime/syslog.hpp"

#include <atomic>
#include <thread>

namespace koheron {

class ListenerManager {
  public:
    ListenerManager();

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
    friend void listening_thread_call(ListeningChannel<socket_type>* listener, ListenerManager* lm);
};

// -----------------------------------------------------------------------------
// Template implementations
// -----------------------------------------------------------------------------

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
void listening_thread_call(ListeningChannel<socket_type>* listener, ListenerManager* lm) {
    listener->is_ready = true;

    while (!lm->should_stop()) {
        int comm_fd = listener->open_communication();

        if (lm->should_stop()) {
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

#endif // __SERVER_CORE_LISTENER_MANAGER_HPP__
