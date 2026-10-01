#ifndef __SERVER_CORE_SOCKETS_HPP__
#define __SERVER_CORE_SOCKETS_HPP__

namespace net {

int create_tcp_listening_socket(unsigned int port);
int set_socket_options(int comm_fd);
int open_tcp_communication(int listen_fd);

int create_unix_listening_socket(const char *unix_sock_path);
int open_unix_communication(int listen_fd);

int shutdown_communication(int listen_fd);

} // namespace net

#endif // __SERVER_CORE_SOCKETS_HPP__
