/// Server configuration
/// (c) Koheron

#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <cstdint>

namespace koheron {
    namespace config {
        namespace log {
            /// Display messages emitted and received
            constexpr bool verbose = false;

            /// Send error messages to stderr
            constexpr bool use_stderr = true;

            /// Send messages to syslog
            constexpr bool syslog = true;
        }

        /// Maximum length of the Unix socket file path
        ///
        /// Note:
        /// 108 is the maximum length on Linux. See:
        /// http://man7.org/linux/man-pages/man7/unix.7.html
        constexpr uint32_t unix_socket_path_len = 108;

        constexpr bool notify_systemd = true;
        constexpr char sd_notify_socket[unix_socket_path_len] = "/run/systemd/notify";

        /// Enable/Disable the Nagle algorithm in the TCP buffer
        constexpr bool tcp_nodelay = true;

        /// TCP listening port
        constexpr unsigned int tcp_port = 36000;
        /// TCP max parallel connections
        constexpr int tcp_worker_connections = 100;

        /// Websocket listening port
        constexpr unsigned int websocket_port = 8080;
        /// Websocket max parallel connections
        constexpr int websocket_worker_connections = 100;

        /// Unix socket file path
        constexpr char unix_socket_path[unix_socket_path_len] = "/var/run/koheron-server.sock";
        /// Unix socket max parallel connections
        constexpr int unix_socket_worker_connections = 100;
    }
} // namespace koheron

#endif // __CONFIG_HPP__