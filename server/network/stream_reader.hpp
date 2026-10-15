// (c) Koheron
#ifndef KOHERON_STREAM_READER_HPP
#define KOHERON_STREAM_READER_HPP

#include "server/runtime/syslog.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <span>
#include <unistd.h>

namespace net {

// Reads until span is filled, returns bytes read (0 = EOF), or -1 on error.
inline ssize_t read_exact(int fd, std::span<std::byte> buf) {
    size_t done = 0;
    while (done < buf.size()) {
        ssize_t n = ::read(fd, buf.data() + done, buf.size() - done);

        if (n == 0) {
            log("TCPSocket: Connection closed by client\n");
            return 0;
        }

        if (n < 0) {
            if (errno == EINTR) { // interrupted -> retry
                continue;
            }

            logf<ERROR>("TCPSocket: Can't receive data (errno={})\n", errno);
            return -1;
        }

        done += static_cast<size_t>(n);
    }

    return static_cast<ssize_t>(done);
}

// Owned exclusively by the session worker. Prefetched bytes remain available
// across command headers and argument reads; large bodies bypass the buffer.
class BufferedStreamReader {
  public:
    ssize_t read_exact(int fd, std::span<std::byte> destination) {
        const auto size = destination.size();
        while (!destination.empty()) {
            if (begin_ != end_) {
                const auto count = std::min(destination.size(), end_ - begin_);
                std::memcpy(destination.data(), buffer_.data() + begin_, count);
                begin_ += count;
                destination = destination.subspan(count);
                continue;
            }
            begin_ = end_ = 0;
            const bool direct = destination.size() >= buffer_.size();
            auto target = direct ? destination : std::span{buffer_};
            const auto count = ::read(fd, target.data(), target.size());
            if (count == 0) return 0;
            if (count < 0) {
                if (errno == EINTR) continue;
                logf<ERROR>("TCPSocket: Can't receive data (errno={})\n", errno);
                return -1;
            }
            const auto received = static_cast<std::size_t>(count);
            if (direct) destination = destination.subspan(received);
            else end_ = received;
        }
        return static_cast<ssize_t>(size);
    }

  private:
    std::array<std::byte, 4096> buffer_;
    std::size_t begin_ = 0;
    std::size_t end_ = 0;
};

} // namespace net
#endif
