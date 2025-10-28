/// Server configuration
/// (c) Koheron

#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <cstdint>

namespace net {
    namespace config {
        /// Maximum length of the Unix socket file path
        ///
        /// Note:
        /// 108 is the maximum length on Linux. See:
        /// http://man7.org/linux/man-pages/man7/unix.7.html
        constexpr uint32_t unix_socket_path_len = 108;

        constexpr bool notify_systemd = true;

        /// Enable/Disable the Nagle algorithm in the TCP buffer
        constexpr bool tcp_nodelay = true;

        /// Set MSG_ZEROCOPY
        constexpr bool use_zerocopy = true;

        /// TCP listening port
        constexpr unsigned int tcp_port = 36000;
        /// TCP max parallel connections
        constexpr int tcp_worker_connections = 100;

        /// Websocket listening port
        constexpr unsigned int websocket_port = 8080;
        /// Websocket max parallel connections
        constexpr int websocket_worker_connections = 200;

        /// Unix socket file path
        constexpr char unix_socket_path[unix_socket_path_len] = "/var/run/koheron-server.sock";
        /// Unix socket max parallel connections
        constexpr int unix_socket_worker_connections = 100;
    }
} // namespace net

#endif // __CONFIG_HPP__
