/// Implementation and specializations of
/// the class ListeningChannel in server.hpp
///
/// (c) Koheron

#include "server.hpp"
#include "session.hpp"

#include <thread>

extern "C" {
  #include <sys/socket.h>   // socket definitions
  #include <arpa/inet.h>    // inet (3) functions
  #include <netinet/tcp.h>
  #include <sys/types.h>    // socket types
  #include <sys/un.h>
}

namespace koheron {

static int create_tcp_listening_socket(unsigned int port, SysLog *syslog)
{
    int listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd_ < 0) {
        syslog->print<PANIC>("Can't open socket\n");
        return -1;
    }

    // To avoid binding error
    // See http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#bind
    int yes = 1;

    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
                  &yes, sizeof(int))==-1) {
        syslog->print<CRITICAL>("Cannot set SO_REUSEADDR\n");
    }

    if (config::tcp_nodelay) {
        int one = 1;

        if (setsockopt(listen_fd_, IPPROTO_TCP, TCP_NODELAY,
                       &one, sizeof(one)) < 0) {
            // This is only considered critical since it is performance
            // related but this doesn't prevent to use the socket
            // so only log the error and keep going ...
            syslog->print<CRITICAL>("Cannot set TCP_NODELAY\n");
        }
    }

    struct sockaddr_in servaddr{};
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);

    // Assign name (address) to socket
    if (bind(listen_fd_, reinterpret_cast<struct sockaddr *>(&servaddr),
             sizeof(servaddr)) < 0) {
        syslog->print<PANIC>("Binding error\n");
        close(listen_fd_);
        return -1;
    }

    return listen_fd_;
}

static int set_socket_options(int comm_fd, SysLog *syslog)
{
    int sndbuf_len = sizeof(uint32_t) * KOHERON_SIG_LEN;

    if (setsockopt(comm_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf_len, sizeof(sndbuf_len)) < 0) {
        syslog->print<CRITICAL>("Cannot set socket send options\n");
        close(comm_fd);
        return -1;
    }

    int rcvbuf_len = KOHERON_READ_STR_LEN;

    if (setsockopt(comm_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_len, sizeof(rcvbuf_len)) < 0) {
        syslog->print<CRITICAL>("Cannot set socket receive options\n");
        close(comm_fd);
        return -1;
    }

    if (config::tcp_nodelay) {
        int one = 1;

        if (setsockopt(comm_fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&one), sizeof(one)) < 0) {
            syslog->print<CRITICAL>("Cannot set TCP_NODELAY\n");
            close(comm_fd);
            return -1;
        }
    }

    return 0;
}

static int open_tcp_communication(int listen_fd, SysLog *syslog)
{
    int comm_fd = accept(listen_fd, nullptr, nullptr);

    if (comm_fd < 0) {
        return comm_fd;
    }

    if (set_socket_options(comm_fd, syslog) < 0) {
        return -1;
    }

    return comm_fd;
}

// ---- TCP ----

template<>
int ListeningChannel<TCP>::init()
{
    number_of_threads = 0;

    if (config::tcp_worker_connections > 0) {
        listen_fd = create_tcp_listening_socket(config::tcp_port, &server->syslog);
        return listen_fd;
    }

    return 0;
}

template<>
void ListeningChannel<TCP>::shutdown()
{
    if (config::tcp_worker_connections > 0) {
        server->syslog.print<INFO>("Closing TCP listener ...\n");

        if (::shutdown(listen_fd, SHUT_RDWR) < 0)
            server->syslog.print<WARNING>("Cannot shutdown socket for TCP listener\n");

        close(listen_fd);
    }
}

template<>
int ListeningChannel<TCP>::open_communication()
{
    return open_tcp_communication(listen_fd, &server->syslog);
}

template<>
bool ListeningChannel<TCP>::is_max_threads()
{
    return number_of_threads >= config::tcp_worker_connections;
}

// ---- WEBSOCK ----

template<>
int ListeningChannel<WEBSOCK>::init()
{
    number_of_threads = 0;

    if (config::websocket_worker_connections > 0) {
        listen_fd = create_tcp_listening_socket(config::websocket_port, &server->syslog);
        return listen_fd;
    }

    return 0;
}

template<>
void ListeningChannel<WEBSOCK>::shutdown()
{
    if (config::websocket_worker_connections > 0) {
        server->syslog.print<INFO>("Closing WebSocket listener ...\n");

        if (::shutdown(listen_fd, SHUT_RDWR) < 0)
            server->syslog.print<WARNING>("Cannot shutdown socket for WebSocket listener\n");

        close(listen_fd);
    }
}

template<>
int ListeningChannel<WEBSOCK>::open_communication()
{
    return open_tcp_communication(listen_fd, &server->syslog);
}

template<>
bool ListeningChannel<WEBSOCK>::is_max_threads()
{
    return number_of_threads >= config::websocket_worker_connections;
}

// ---- UNIX ----

static int create_unix_listening_socket(const char *unix_sock_path, SysLog *syslog)
{
    struct sockaddr_un local{};

    int listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);

    if (listen_fd_ < 0) {
        syslog->print<PANIC>("Can't open Unix socket\n");
        return -1;
    }

    memset(&local, 0, sizeof(struct sockaddr_un));
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, unix_sock_path);
    unlink(local.sun_path);
    auto len = strlen(local.sun_path) + sizeof(local.sun_family);

    if (bind(listen_fd_, reinterpret_cast<struct sockaddr *>(&local), len) < 0) {
        syslog->print<PANIC>("Unix socket binding error\n");
        close(listen_fd_);
        return -1;
    }

    return listen_fd_;
}

template<>
int ListeningChannel<UNIX>::init()
{
    number_of_threads = 0;

    if (config::unix_socket_worker_connections > 0) {
        listen_fd = create_unix_listening_socket(config::unix_socket_path, &server->syslog);
        return listen_fd;
    }

    return 0;
}

template<>
void ListeningChannel<UNIX>::shutdown()
{
    if (config::unix_socket_worker_connections > 0) {
        server->syslog.print<INFO>("Closing Unix listener ...\n");

        if (::shutdown(listen_fd, SHUT_RDWR) < 0)
            server->syslog.print<WARNING>("Cannot shutdown socket for Unix listener\n");

        close(listen_fd);
    }
}

template<>
int ListeningChannel<UNIX>::open_communication()
{
    struct sockaddr_un remote{};
    uint32_t t = sizeof(remote);
    return accept(listen_fd, reinterpret_cast<struct sockaddr *>(&remote), &t);
}

template<>
bool ListeningChannel<UNIX>::is_max_threads()
{
    return number_of_threads >= config::unix_socket_worker_connections;
}

} // namespace koheron
