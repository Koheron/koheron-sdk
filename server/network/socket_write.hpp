#ifndef KOHERON_SOCKET_WRITE_HPP
#define KOHERON_SOCKET_WRITE_HPP

#include <cerrno>
#include <cstddef>
#include <limits>
#include <span>
#include <sys/socket.h>
#include <sys/uio.h>

namespace net {

// Mutates only the caller's iovec descriptors. Borrowed bytes remain untouched.
inline int write_iovecs(int fd, std::span<iovec> parts, int flags) {
    std::size_t total = 0;
    for (const auto& part : parts) {
        if (part.iov_len > static_cast<std::size_t>(std::numeric_limits<int>::max()) - total)
            return -1;
        total += part.iov_len;
    }
    std::size_t first = 0;
    while (first < parts.size()) {
        if (parts[first].iov_len == 0) { ++first; continue; }
        msghdr message{};
        message.msg_iov = parts.data() + first;
        message.msg_iovlen = parts.size() - first;
        const auto n = ::sendmsg(fd, &message, flags);
        if (n == 0) return 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        auto remaining = static_cast<std::size_t>(n);
        while (remaining != 0) {
            if (remaining >= parts[first].iov_len) {
                remaining -= parts[first].iov_len;
                ++first;
            } else {
                parts[first].iov_base = static_cast<std::byte*>(parts[first].iov_base) + remaining;
                parts[first].iov_len -= remaining;
                remaining = 0;
            }
        }
    }
    return static_cast<int>(total);
}

} // namespace net
#endif
