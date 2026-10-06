// Exercise production framing and sendmsg with real sockets and forced short writes.
#include "server/network/websocket.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

using Bytes = std::vector<unsigned char>;
constexpr std::size_t chunk_size = net::WEBSOCK_SEND_BUF_LEN - 10;

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

enum class Mode { normal, partial, failure, zero };
struct Trace {
    std::span<const unsigned char> h, p;
    Mode mode = Mode::normal;
    unsigned calls = 0;
    std::size_t borrowed_bytes = 0;
    bool copied = false;
    bool missing_nosignal = false;
} trace;

bool contains(std::span<const unsigned char> source, const iovec& part) {
    const auto base = reinterpret_cast<std::uintptr_t>(source.data());
    const auto pos = reinterpret_cast<std::uintptr_t>(part.iov_base);
    return pos >= base && pos - base <= source.size() &&
           part.iov_len <= source.size() - (pos - base);
}

extern "C" ssize_t __real_sendmsg(int fd, const msghdr* msg, int flags);
extern "C" ssize_t __wrap_sendmsg(int fd, const msghdr* msg, int flags) {
    ++trace.calls;
    trace.missing_nosignal |= !(flags & MSG_NOSIGNAL);
    for (std::size_t i = 0; i < msg->msg_iovlen; ++i) {
        const auto& part = msg->msg_iov[i];
        if (part.iov_len == 0) continue;
        if (contains(trace.h, part) || contains(trace.p, part)) {
            trace.borrowed_bytes += part.iov_len;
        } else if (part.iov_len > 10) {
            trace.copied = true;
        }
    }

    if (trace.mode == Mode::partial && (trace.calls == 1 || trace.calls == 4)) {
        errno = EINTR;
        return -1;
    }
    if (trace.calls == 4 && trace.mode == Mode::failure) {
        errno = EPIPE;
        return -1;
    }
    if (trace.calls == 4 && trace.mode == Mode::zero) return 0;
    if (trace.mode == Mode::normal) return __real_sendmsg(fd, msg, flags);

    // Restrict the real write across iovec boundaries without modifying the
    // caller's vectors. Cuts fall inside both framing and application headers.
    constexpr std::array<std::size_t, 6> limits{1, 2, 3, 8, 31, 4093};
    auto remaining = limits[(trace.calls - 1) % limits.size()];
    std::array<iovec, 3> parts{};
    if (msg->msg_iovlen > parts.size()) {
        errno = EINVAL;
        return -1;
    }
    std::size_t count = 0;
    for (std::size_t i = 0; i < msg->msg_iovlen && remaining > 0; ++i) {
        parts[count] = msg->msg_iov[i];
        parts[count].iov_len = std::min(parts[count].iov_len, remaining);
        remaining -= parts[count].iov_len;
        ++count;
    }
    auto shortened = *msg;
    shortened.msg_iov = parts.data();
    shortened.msg_iovlen = count;
    return __real_sendmsg(fd, &shortened, flags);
}

struct Peer {
    int fd[2];
    std::thread reader;
    Bytes bytes;
    bool read_error = false;

    explicit Peer(bool capture = true) {
        check(::socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0, "socketpair");
        const int buffer_size = 4096;
        check(::setsockopt(fd[0], SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size)) == 0,
              "send buffer");
        if (capture) reader = std::thread([this] {
            std::array<unsigned char, 8192> buffer{};
            while (true) {
                const auto n = ::recv(fd[1], buffer.data(), buffer.size(), 0);
                if (n > 0) bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + n);
                else if (n < 0 && errno == EINTR) continue;
                else {
                    read_error = n < 0;
                    break;
                }
            }
        });
    }

    void finish() {
        ::shutdown(fd[0], SHUT_WR);
        if (reader.joinable()) reader.join();
        check(!read_error, "receiver failed");
    }

    ~Peer() {
        ::shutdown(fd[0], SHUT_RDWR);
        if (fd[1] >= 0) ::shutdown(fd[1], SHUT_RDWR);
        if (reader.joinable()) reader.join();
        ::close(fd[0]);
        if (fd[1] >= 0) ::close(fd[1]);
    }
};

Bytes pattern(std::size_t size, unsigned seed) {
    Bytes bytes(size);
    for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<unsigned char>(37 * i + seed);
    return bytes;
}

void check_frames(const Bytes& wire, const Bytes& h, const Bytes& p) {
    std::vector<std::pair<unsigned, std::size_t>> frames;
    if (h.size() + p.size() <= chunk_size) frames.emplace_back(0x82, h.size() + p.size());
    else {
        std::size_t sent = 0;
        do {
            const auto size = std::min(h.size() - sent, chunk_size);
            const auto flags = (sent == 0 ? 2 : 0) |
                               (sent + size == h.size() && p.empty() ? 0x80 : 0);
            frames.emplace_back(flags, size);
            sent += size;
        } while (sent < h.size());
        for (sent = 0; sent < p.size();) {
            const auto size = std::min(p.size() - sent, chunk_size);
            frames.emplace_back(sent + size == p.size() ? 0x80 : 0, size);
            sent += size;
        }
    }

    Bytes decoded;
    std::size_t pos = 0;
    for (auto [flags, size] : frames) {
        check(wire.size() - pos >= 2, "truncated frame header");
        check(wire[pos++] == flags, "opcode, FIN or reserved bits changed");
        const auto length_code = wire[pos++];
        check(length_code == (size <= 125 ? size : size <= 65535 ? 126 : 127),
              "incorrect or masked length encoding");
        if (length_code >= 126) {
            const std::size_t width = length_code == 126 ? 2 : 8;
            check(wire.size() - pos >= width, "truncated extended length");
            std::uint64_t length = 0;
            for (std::size_t i = 0; i < width; ++i) length = (length << 8) | wire[pos++];
            check(length == size, "incorrect extended length");
        }
        check(wire.size() - pos >= size, "truncated frame payload");
        decoded.insert(decoded.end(), wire.begin() + pos, wire.begin() + pos + size);
        pos += size;
    }
    check(pos == wire.size(), "extra frame bytes");
    auto expected = h;
    expected.insert(expected.end(), p.begin(), p.end());
    check(decoded == expected, "payload changed, duplicated or reordered");
}

void send_case(std::size_t hsize, std::size_t psize, bool single = false,
               Mode mode = Mode::normal) {
    const auto h = pattern(hsize, 11), p = pattern(psize, 71);
    trace = {h, p, mode};
    Peer peer;
    net::WebSocket socket;
    socket.set_id(peer.fd[0]);
    const int n = single ? socket.send(h) : socket.send(h, p);
    peer.finish();
    const auto expected_count = hsize + psize <= chunk_size ? peer.bytes.size() : hsize + psize;
    check(n == static_cast<int>(expected_count), "send return value changed");
    check_frames(peer.bytes, h, p);
    check(trace.calls > 0 && !trace.copied, "reply copied into a staging buffer");
    check(hsize + psize == 0 || trace.borrowed_bytes > 0, "application buffers not borrowed");
    check(!trace.missing_nosignal, "sendmsg missing MSG_NOSIGNAL");
    check(!socket.is_closed(), "successful send closed connection");
}

void boundaries() {
    for (std::size_t size : {0U, 1U, 125U, 126U, 65535U, 65536U,
                            static_cast<unsigned>(chunk_size)}) {
        send_case(size, 0, true);
        send_case(0, size);
        if (size >= 8) send_case(8, size - 8);
    }
}

void fragmented() {
    send_case(12, 3 * chunk_size + 19);
    send_case(8, chunk_size - 7);
    send_case(0, chunk_size + 1);
    send_case(2 * chunk_size + 9, 0);
    send_case(2 * chunk_size + 9, chunk_size + 3);
}

void partial() {
    send_case(32, 257, false, Mode::partial);
    send_case(0, 126, false, Mode::partial);
    send_case(125, 0, true, Mode::partial);
    send_case(12, chunk_size + 31, false, Mode::partial);
}

void failures() {
    for (Mode mode : {Mode::failure, Mode::zero}) {
        const auto h = pattern(16, 11), p = pattern(128, 71);
        trace = {h, p, mode};
        Peer peer;
        net::WebSocket socket;
        socket.set_id(peer.fd[0]);
        check(socket.send(h, p) == (mode == Mode::failure ? -1 : 0), "failure not propagated");
        check(socket.is_closed(), "failed frame left connection open");
        const auto calls = trace.calls;
        check(socket.send(h, p) == 0 && socket.send(h) == 0, "send retried a failed frame");
        check(trace.calls == calls, "closed connection reached sendmsg");
        peer.finish();
        Bytes expected{0x82, 126, 0, 144};
        expected.insert(expected.end(), h.begin(), h.end());
        expected.insert(expected.end(), p.begin(), p.end());
        check(peer.bytes.size() == 6 && std::equal(peer.bytes.begin(), peer.bytes.end(), expected.begin()),
              "partial frame prefix changed before failure");
    }

    Peer peer(false);
    ::close(peer.fd[1]); peer.fd[1] = -1;
    net::WebSocket socket;
    socket.set_id(peer.fd[0]);
    const auto bytes = pattern(12, 11);
    trace = {bytes, {}, Mode::normal};
    check(socket.send(bytes) == -1 && socket.is_closed(), "disconnected peer not detected");
}

void single_limit() {
    const auto bytes = pattern(chunk_size + 1, 11);
    trace = {bytes, {}, Mode::normal};
    Peer peer;
    net::WebSocket socket;
    socket.set_id(peer.fd[0]);
    check(socket.send(bytes) == -1 && trace.calls == 0, "single-range limit changed");
    check(!socket.is_closed(), "oversized send closed connection");
    check(socket.send(std::span{bytes}.first(1)) == 3, "connection lost after oversized send");
    check(socket.exit() == 2, "close frame send failed");
    peer.finish();
    check(peer.bytes == Bytes{0x82, 1, bytes[0], 0x88, 0}, "reply or close frame changed");
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected test name");
        const std::string test = argv[1];
        if (test == "boundaries") boundaries();
        else if (test == "fragmented") fragmented();
        else if (test == "partial") partial();
        else if (test == "failures") failures();
        else if (test == "single-limit") single_limit();
        else throw std::runtime_error("unknown test");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
