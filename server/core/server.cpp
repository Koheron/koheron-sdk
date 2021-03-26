/// Implementation of server.hpp
///
/// (c) Koheron

#include "server.hpp"

#include <chrono>
#include <cstdlib>

#include "commands.hpp"
#include "session.hpp"
#include "session_manager.hpp"

extern "C" {
  #include <sys/un.h>
}

namespace koheron {

Server::Server()
: signal_handler()
, tcp_listener(this)
, websock_listener(this)
, unix_listener(this)
, driver_manager(this)
, syslog()
, session_manager(driver_manager, syslog)
{
    if (signal_handler.init(this) < 0) {
        exit(EXIT_FAILURE);
    }

    if (driver_manager.init() < 0) {
        exit(EXIT_FAILURE);
    }

    exit_comm.store(false);
    exit_all.store(false);

    if (config::tcp_worker_connections > 0) {
        if (tcp_listener.init() < 0) {
            exit(EXIT_FAILURE);
        }
    }

    if (config::websocket_worker_connections > 0) {
        if (websock_listener.init() < 0) {
            exit(EXIT_FAILURE);
        }
    }

    if (config::unix_socket_worker_connections > 0) {
        if (unix_listener.init() < 0) {
            exit(EXIT_FAILURE);
        }
    }
}

// This cannot be done in the destructor
// since it is called after the "delete config"
// at the end of the main()
void Server::close_listeners()
{
    exit_comm.store(true);
    session_manager.exit_comm();
    tcp_listener.shutdown();
    websock_listener.shutdown();
    unix_listener.shutdown();
    join_listeners_workers();
}

int Server::start_listeners_workers()
{
    if (tcp_listener.start_worker() < 0) {
        return -1;
    }
    if (websock_listener.start_worker() < 0) {
        return -1;
    }
    if (unix_listener.start_worker() < 0) {
        return -1;
    }
    return 0;
}

void Server::join_listeners_workers()
{
    tcp_listener.join_worker();
    websock_listener.join_worker();
    unix_listener.join_worker();
}

bool Server::is_ready()
{
    bool ready = true;
    if (config::tcp_worker_connections > 0) {
        ready = ready && tcp_listener.is_ready;
    }
    if (config::websocket_worker_connections > 0) {
        ready = ready && websock_listener.is_ready;
    }
    if (config::unix_socket_worker_connections > 0) {
        ready = ready && unix_listener.is_ready;
    }
    return ready;
}

void Server::notify_systemd_ready()
{
    // We call directly the notification socket as uwsgi does:
    // https://github.com/unbit/uwsgi/blob/master/core/notify.c

    struct sockaddr_un sd_sun{};
    struct msghdr msg{};
    const char *state;

    int sd_notif_fd = socket(AF_UNIX, SOCK_DGRAM|SOCK_CLOEXEC, 0);

    if (sd_notif_fd < 0) {
        syslog.print<WARNING>("Cannot open notification socket\n");
        return;
    }

    int len = strlen(config::sd_notify_socket);
    memset(&sd_sun, 0, sizeof(struct sockaddr_un));
    sd_sun.sun_family = AF_UNIX;
    memcpy(sd_sun.sun_path, config::sd_notify_socket, std::min(static_cast<size_t>(len), sizeof(sd_sun.sun_path)));

    if (sd_sun.sun_path[0] == '@') {
        sd_sun.sun_path[0] = 0;
    }

    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_iov = reinterpret_cast<struct iovec*>(malloc(sizeof(struct iovec) * 3));

    if (msg.msg_iov == nullptr) {
        syslog.print<WARNING>("Cannot allocate msg.msg_iov\n");
        goto exit_notification_socket;
    }

    memset(msg.msg_iov, 0, sizeof(struct iovec) * 3);
    msg.msg_name = &sd_sun;
    msg.msg_namelen = sizeof(struct sockaddr_un) - (sizeof(sd_sun.sun_path) - static_cast<unsigned int>(len));

    state = "STATUS=Koheron server is ready\nREADY=1\n";
    msg.msg_iov[0].iov_base = const_cast<char *>(state);
    msg.msg_iov[0].iov_len = strlen(state);
    msg.msg_iovlen = 1;

    if (sendmsg(sd_notif_fd, &msg, MSG_NOSIGNAL) < 0) {
        syslog.print<WARNING>("Cannot send notification to systemd.\n");
    }

    free(msg.msg_iov);

exit_notification_socket:
    if (::shutdown(sd_notif_fd, SHUT_RDWR) < 0) {
        syslog.print<WARNING>("Cannot shutdown notification socket\n");
    }

    close(sd_notif_fd);
}

int Server::run()
{
    bool ready_notified = false;

    if (start_listeners_workers() < 0) {
        return -1;
    }

    while (true) {
        if (!ready_notified && is_ready()) {
            syslog.print<INFO>("Koheron server ready\n");
            if (config::notify_systemd) {
                notify_systemd_ready();
            }
            ready_notified = true;
        }

        if (signal_handler.interrupt() || exit_all) {
            syslog.print<INFO>("Interrupt received, killing Koheron server ...\n");
            session_manager.delete_all();
            close_listeners();
            syslog.close();
            return 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}

} // namespace koheron
