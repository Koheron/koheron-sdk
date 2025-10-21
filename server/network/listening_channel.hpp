/// Listening channel types and utilities
///
/// (c) Koheron

#ifndef __SERVER_CORE_LISTENING_CHANNEL_HPP__
#define __SERVER_CORE_LISTENING_CHANNEL_HPP__

#include "server/network/configs/server_definitions.hpp"
#include "server/runtime/syslog.hpp"

#include <atomic>
#include <thread>

namespace net {

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

    void join_worker();
    int open_communication();

    int listen_fd;
    std::atomic<int> number_of_threads; // Number of sessions using the channel
    std::atomic<bool> is_ready;
    std::thread listening_thread;
};

template<int socket_type>
void ListeningChannel<socket_type>::join_worker() {
    if (listen_fd >= 0 && listening_thread.joinable()) {
        listening_thread.join();
    }
}

} // namespace net

#endif // __SERVER_CORE_LISTENING_CHANNEL_HPP__
