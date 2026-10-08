#include "server/network/session.hpp"
#include "server/network/socket_write.hpp"
#include "server/network/websocket.hpp"

#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

using Bytes = std::vector<unsigned char>;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Capture : net::Session {
    Capture() : Session(net::TCP) {}
    Bytes bytes;
    std::vector<std::span<const std::byte>> borrowed;
    bool segmented = false;
    std::size_t staging_bytes() const { return send_buffer.size(); }
    void shutdown() override {}
    int init_socket() override { return 0; }
    int exit_socket() override { return 0; }
    int read_command(net::Command&) override { return 0; }
    int write_bytes(std::span<const std::byte> part) override {
        const auto* p = reinterpret_cast<const unsigned char*>(part.data());
        bytes.insert(bytes.end(), p, p + part.size());
        return static_cast<int>(part.size());
    }
    int write_segments(std::span<const std::span<const std::byte>> parts) override {
        segmented = true;
        int n = 0;
        for (auto part : parts) {
            if (part.size() >= net::mixed_reply_min_bytes) borrowed.push_back(part);
            n += write_bytes(part);
        }
        return n;
    }
    int send_iov(std::span<const std::byte> h, std::span<const std::byte> p, int) override {
        return write_bytes(h) + write_bytes(p);
    }
};

template<class T>
void compare(T&& reply, std::span<const std::byte> first, std::span<const std::byte> second = {}) {
    Capture session;
    std::pmr::vector<unsigned char> packed;
    net::CommandBuilder builder;
    builder.reset_into(packed);
    builder.write_header(0xbeef, 0x1234);
    builder.push(reply);
    check(session.send(0xbeef, 0x1234, reply) == static_cast<int>(packed.size()), "send return value");
    check(std::equal(packed.begin(), packed.end(), session.bytes.begin(), session.bytes.end()), "wire bytes changed");
    check(session.segmented, "large mixed reply was packed");
    auto borrowed = [&](auto source) {
        return std::ranges::any_of(session.borrowed, [&](auto part) {
            return part.data() == source.data() && part.size() == source.size();
        });
    };
    check(borrowed(first), "first container copied");
    if (!second.empty()) check(borrowed(second), "second container copied");
    check(session.staging_bytes() == packed.size() - first.size() - second.size(), "payload remained in staging buffer");
}

void serialization() {
    std::array<uint32_t, 2048> array{};
    std::vector<uint16_t> vector(4096);
    for (std::size_t i = 0; i < array.size(); ++i) array[i] = static_cast<uint32_t>(31 * i + 17);
    for (std::size_t i = 0; i < vector.size(); ++i) vector[i] = static_cast<uint16_t>(13 * i + 5);
    // Metadata after a borrowed part grows the vector beyond its initial storage.
    auto nested = std::tuple{int32_t{-1234}, true, std::tuple{std::cref(array).get(),
        std::string(12000, 'z'), std::span<const uint16_t>{vector}}, 1.25,
        std::complex<float>{2.5f, -3.0f}, std::array<uint8_t, 0>{}, uint16_t{0xabcd}};
    compare(nested, std::as_bytes(std::span{std::get<0>(std::get<2>(nested))}), std::as_bytes(std::span{vector}));
    auto vectors = std::tuple<const std::vector<uint16_t>&, uint8_t, const std::array<uint32_t, 2048>&>{vector, 19, array};
    compare(vectors, std::as_bytes(std::span{vector}), std::as_bytes(std::span{array}));
    Capture small;
    small.send(1, 2, std::tuple{uint32_t{3}, std::vector<uint8_t>(4095), uint16_t{4}});
    check(!small.segmented, "small payload entered scatter path");
    Capture many;
    auto lots = std::tuple{vector, vector, vector, vector, vector, vector, vector, vector,
                          vector, vector, vector, vector, vector, vector, vector, vector};
    many.send(1, 2, lots);
    check(!many.segmented, "descriptor limit did not fall back to packing");
    struct LegacyCapture : Capture {
        int write_segments(std::span<const std::span<const std::byte>> parts) override {
            return net::Session::write_segments(parts);
        }
    } legacy;
    std::pmr::vector<unsigned char> packed;
    net::CommandBuilder builder;
    builder.reset_into(packed); builder.write_header(1, 2); builder.push(vectors);
    legacy.send(1, 2, vectors);
    check(std::equal(packed.begin(), packed.end(), legacy.bytes.begin(), legacy.bytes.end()), "legacy transport fallback changed bytes");
}

enum class Mode { normal, partial, interrupt, failure, zero };
// Keep fault-injection state observable across the linker-wrapped call.
std::atomic<Mode> mode{Mode::normal};
std::atomic<unsigned> calls{0};
#ifdef __USE_TIME_BITS64
// glibc redirects sendmsg to this symbol on 32-bit time64 systems.
extern "C" ssize_t __real___sendmsg64(int, const msghdr*, int);
#define __real_sendmsg __real___sendmsg64
#define __wrap_sendmsg __wrap___sendmsg64
#endif
extern "C" ssize_t __real_sendmsg(int, const msghdr*, int);
extern "C" ssize_t __wrap_sendmsg(int fd, const msghdr* message, int flags) {
    ++calls;
    check(flags == MSG_NOSIGNAL, "mixed reply enabled asynchronous zero-copy or lost NOSIGNAL");
    if ((mode == Mode::interrupt && calls == 1) || (mode == Mode::partial && calls == 2)) {
        errno = EINTR;
        return -1;
    }
    if (calls == 3 && mode == Mode::failure) { errno = EPIPE; return -1; }
    if (calls == 3 && mode == Mode::zero) return 0;
    if (mode == Mode::normal || mode == Mode::interrupt) return __real_sendmsg(fd, message, flags);
    std::array<iovec, net::max_reply_parts + 1> parts{};
    check(message->msg_iovlen <= parts.size(), "descriptor overflow");
    std::size_t remaining = calls % 3 == 0 ? 4093 : calls % 3 == 1 ? 1 : 17, count = 0;
    for (std::size_t i = 0; i < message->msg_iovlen && remaining != 0; ++i) {
        parts[count] = message->msg_iov[i];
        parts[count].iov_len = std::min(parts[count].iov_len, remaining);
        remaining -= parts[count++].iov_len;
    }
    auto cut = *message;
    cut.msg_iov = parts.data(); cut.msg_iovlen = count;
    return __real_sendmsg(fd, &cut, flags);
}

void limits() {
    std::byte byte{};
    const auto max = static_cast<std::size_t>(std::numeric_limits<int>::max());
    std::array<iovec, 2> overflowing{{{&byte, max}, {&byte, 1}}};
    calls = 0;
    check(net::write_iovecs(-1, overflowing, MSG_NOSIGNAL) == -1 && calls == 0, "overflow reached socket");
    net::WebSocket ws;
    // Reuse a valid range to exceed the aggregate limit without touching 2 GiB.
    const auto part_size = max / net::max_reply_parts + 1;
    auto storage = std::make_unique_for_overwrite<std::byte[]>(part_size);
    std::array<std::span<const std::byte>, net::max_reply_parts> parts;
    parts.fill({storage.get(), part_size});
    check(ws.send_parts(parts) == -1 && calls == 0, "WebSocket overflow reached socket");
    std::array<std::span<const std::byte>, net::max_reply_parts + 1> too_many{};
    check(ws.send_parts(too_many) == -1 && calls == 0, "too many descriptors reached socket");
    check(!ws.is_closed(), "validation error closed connection");
}

void transport(bool websocket, Mode selected, std::size_t size) {
    mode = selected; calls = 0;
    int fd[2];
    check(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0, "socketpair");
    const Bytes prefix{0, 1, 2, 3, 4, 5, 6}, suffix{91, 92, 93};
    Bytes data(size);
    for (std::size_t i = 0; i < size; ++i) data[i] = (31 * i + 17) & 255;
    const std::array<std::span<const std::byte>, 5> parts{{std::as_bytes(std::span{prefix}), {},
        std::as_bytes(std::span{data}), {}, std::as_bytes(std::span{suffix})}};
    Bytes received;
    std::thread reader([&] {
        std::array<unsigned char, 4096> buffer{};
        ssize_t n;
        while ((n = recv(fd[1], buffer.data(), buffer.size(), 0)) > 0)
            received.insert(received.end(), buffer.begin(), buffer.begin() + n);
    });
    net::WebSocket ws; ws.set_id(fd[0]);
    int result;
    if (websocket) result = ws.send_parts(parts);
    else {
        std::array<iovec, 5> iov{};
        for (std::size_t i = 0; i < parts.size(); ++i)
            iov[i] = {const_cast<std::byte*>(parts[i].data()), parts[i].size()};
        result = net::write_iovecs(fd[0], iov, MSG_NOSIGNAL);
    }
    shutdown(fd[0], SHUT_WR); reader.join(); close(fd[0]); close(fd[1]);
    if (selected == Mode::failure || selected == Mode::zero) {
        if (result != (selected == Mode::failure ? -1 : 0)) {
            std::cerr << "ws=" << websocket << " mode=" << static_cast<int>(selected)
                      << " calls=" << calls << " result=" << result << '\n';
        }
        check(result == (selected == Mode::failure ? -1 : 0), "send error not propagated");
        if (websocket) check(ws.is_closed(), "failed WebSocket left open");
        return;
    }
    Bytes expected = prefix; expected.insert(expected.end(), data.begin(), data.end());
    expected.insert(expected.end(), suffix.begin(), suffix.end());
    if (websocket) {
        Bytes decoded;
        std::size_t offset = 0; bool first = true;
        while (offset < received.size()) {
            const auto flags = received[offset++], length = received[offset++];
            check((flags & 15) == (first ? 2 : 0) && !(length & 128), "invalid WebSocket frame");
            uint64_t n = length;
            if (n >= 126) {
                const auto width = n == 126 ? 2u : 8u;
                n = 0;
                for (unsigned i = 0; i < width; ++i) n = (n << 8) | received.at(offset++);
            }
            check(n <= received.size() - offset, "truncated frame");
            decoded.insert(decoded.end(), received.begin() + offset, received.begin() + offset + n);
            offset += n;
            check(bool(flags & 128) == (offset == received.size()), "incorrect FIN bit");
            first = false;
        }
        check(decoded == expected, "WebSocket wire bytes changed");
    } else check(received == expected, "stream wire bytes changed");
    check(result == static_cast<int>(received.size()), "incorrect byte count");
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected case");
        const std::string test = argv[1];
        if (test == "serialization") serialization();
        else if (test == "limits") limits();
        else if (test == "boundaries") {
            for (auto size : {0u, 115u, 116u, 65525u, 65526u, 262123u, 262124u, 262125u, 1048576u})
                for (bool ws : {false, true}) transport(ws, Mode::normal, size);
        } else if (test == "partial") {
            for (bool ws : {false, true}) for (auto m : {Mode::partial, Mode::interrupt})
                transport(ws, m, 262200);
        } else if (test == "failures") {
            for (bool ws : {false, true}) for (auto m : {Mode::failure, Mode::zero}) transport(ws, m, 4096);
        } else throw std::runtime_error("unknown case");
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
