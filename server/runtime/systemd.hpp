#include <memory>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cstddef>

namespace systemd {

inline void notify_ready() {
    const char* sock = std::getenv("NOTIFY_SOCKET");

    if (!sock || !*sock) {
        return;
    }

    int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);

    if (fd < 0) {
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

    static constexpr char msg[] = "READY=1";
    (void)::sendto(fd, msg, sizeof(msg) - 1, 0, reinterpret_cast<sockaddr*>(&sa), addrlen);

    ::close(fd);
}

} // namespace systemd