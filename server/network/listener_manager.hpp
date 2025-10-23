/// Listeners orchestration
///
/// (c) Koheron

#ifndef __SERVER_CORE_LISTENER_MANAGER_HPP__
#define __SERVER_CORE_LISTENER_MANAGER_HPP__

#include "server/network/configs/server_definitions.hpp"
#include "server/network/configs/config.hpp"
#include "server/network/listening_channel.hpp"
#include "server/network/session_manager.hpp"
#include "server/network/socket_session.hpp"

#include "server/runtime/services.hpp"
#include "server/runtime/syslog.hpp"
#include "server/runtime/signal_handler.hpp"
#include "server/runtime/systemd.hpp"

#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>

namespace net {

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
};

// -----------------------------------------------------------------------------
// Template implementations
// -----------------------------------------------------------------------------

template<int socket_type>
void session_thread_call(int comm_fd, ListeningChannel<socket_type>* listener) {
    listener->number_of_threads++;
    auto& sm = services::require<SessionManager>();
    auto sid = sm.template create_session<socket_type>(comm_fd);
    auto session = static_cast<SocketSession<socket_type>*>(&sm.get_session(sid));

    if (session->run() < 0) {
        log<ERROR>("An error occured during session\n");
    }

    sm.delete_session(sid);
    listener->number_of_threads--;
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
            logf<CRITICAL>("Connection to client rejected [socket_type = {}]\n", socket_type);
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

// -----------------------------------------------------------------------------
// Run => Main server loop
// -----------------------------------------------------------------------------

inline int run_server(std::string_view on_ready_msg="server ready\n") {
    using clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;

    // Config periodic rates dump
    constexpr auto dump_period = 1s; // adjust cadence
    auto next_dump = clock::now() + dump_period;
    fs::path dump_path = "/run/koheron/sessions_rates.json";

    auto signal_handler = rt::SignalHandler();

    if (signal_handler.init() < 0) {
        return EXIT_FAILURE;
    }

    auto lm = ListenerManager();

    if (lm.start() < 0) {
        return EXIT_FAILURE;
    }

    bool ready_notified = false;
    auto& sm = services::require<net::SessionManager>();

    while (true) {
        if (!ready_notified && lm.is_ready()) {
            log(on_ready_msg.data());

            if constexpr (net::config::notify_systemd) {
                rt::systemd::notify_ready(on_ready_msg);
            }

            ready_notified = true;
        }

        if (signal_handler.interrupt()) {
            log("Interrupt received, killing Koheron server ...\n");
            lm.shutdown();
            return EXIT_SUCCESS;
        }

        // Periodic dump of session rate
        const auto now = clock::now();
        if (now >= next_dump) {
            do {
                const bool ok = sm.dump_rates(dump_path);
                if (!ok) {
                    logf<WARNING>("dump_rates: failed to write '{}'\n", dump_path);
                }
                next_dump += dump_period;
            } while (now >= next_dump);
        }

        // Sleep until sooner of: next dump or 50 ms tick
        constexpr auto tick = 50ms;
        const auto until_next_dump = std::max(std::chrono::duration_cast<std::chrono::milliseconds>(next_dump - now), 0ms);
        std::this_thread::sleep_for(std::min(tick, until_next_dump));
    }

    return EXIT_SUCCESS;
}

} // namespace net

#endif // __SERVER_CORE_LISTENER_MANAGER_HPP__
