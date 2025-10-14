// (c) Koheron

#include "server/runtime/systemd.hpp"
#include "server/runtime/syslog.hpp"

#include <memory>
#include <string_view>
#include <string>
#include <cstring>
#include <cstddef>
#include <cstdlib>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace rt::systemd {

void notify_ready(std::string_view status_msg) {
    const char* sock = std::getenv("NOTIFY_SOCKET");

    if (!sock || !*sock) {
        return;
    }

    int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);

    if (fd < 0) {
        rt::print<WARNING>("Cannot open notification socket\n");
        return;
    }

    sockaddr_un sa{};
    sa.sun_family = AF_UNIX;
    std::size_t len = std::strlen(sock);

    if (sock[0] == '@') {
        sa.sun_path[0] = '\0';

        if (len > 1) {
            std::memcpy(sa.sun_path + 1, sock + 1, len - 1);
        }
    } else {
        std::strncpy(sa.sun_path, sock, sizeof(sa.sun_path) - 1);
        len += 1;
    }

    socklen_t addrlen = offsetof(sockaddr_un, sun_path) + len;

    std::string payload;

    if (!status_msg.empty()) {
        payload.reserve(status_msg.size() + 16);
        payload.append("STATUS=");
        payload.append(status_msg);
        payload.push_back('\n');
    }

    payload.append("READY=1\n");

    (void)::sendto(fd, payload.data(), payload.size(), 0,
                   reinterpret_cast<sockaddr*>(&sa), addrlen);

    ::close(fd);
}

} // namespace rt::systemd

