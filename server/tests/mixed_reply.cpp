#include "server/network/session.hpp"
#include "server/network/socket_write.hpp"
#include "server/network/socket_session.hpp"
#include "server/network/websocket.hpp"

#include <algorithm>
#include <atomic>
#include <iostream>
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
    bool closed() const { return status == CLOSED; }
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

void fixed_replies() {
    auto compare_fixed = [](const auto& reply, bool fixed = true) {
        Capture session;
        std::pmr::vector<unsigned char> expected;
        net::CommandBuilder builder;
        builder.reset_into(expected);
        builder.write_header(0xbeef, 0x1234);
        builder.push(reply);
        check(session.send(0xbeef, 0x1234, reply) == static_cast<int>(expected.size()), "fixed reply length");
        check(session.bytes == Bytes(expected.begin(), expected.end()), "fixed reply wire bytes");
        check(session.staging_bytes() == (fixed ? 0 : expected.size()), "fixed reply size boundary");
        check(session.rates()[1].total_bytes == static_cast<int64_t>(expected.size()), "fixed reply TX accounting");
    };
    compare_fixed(uint32_t{0x12345678});
    compare_fixed(std::tuple{true, false, int8_t{-128}, uint8_t{255}, int16_t{-32768},
        uint16_t{65535}, std::numeric_limits<int32_t>::min(), UINT32_MAX,
        std::numeric_limits<int64_t>::min(), UINT64_MAX});
    compare_fixed(std::tuple{std::tuple{1.25f, -0.f, std::complex<float>{2.5f, -3.f}},
        std::tuple{1.25, -0., std::complex<double>{-4., 5.}},
        std::numeric_limits<float>::infinity(), -std::numeric_limits<double>::infinity(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<double>::denorm_min()});
    compare_fixed(std::tuple{scicpp::units::hertz<double>{10.}, scicpp::units::second<float>{2.f}});
    const double scalar = -1.5;
    compare_fixed(std::tuple<const double&, std::tuple<uint32_t, bool>>{scalar, {7, true}});
    auto integers = []<std::size_t... I>(std::index_sequence<I...>) {
        return std::tuple{static_cast<uint64_t>(I)...};
    };
    compare_fixed(integers(std::make_index_sequence<63>{})); // 512 bytes including header
    compare_fixed(integers(std::make_index_sequence<64>{}), false);
    compare_fixed(std::tuple{}, false);
    compare_fixed(std::tuple{uint32_t{3}, std::tuple{}}, false);
    compare_fixed(std::tuple{uint32_t{3}, std::string{"dynamic"}}, false);

    Capture variadic;
    check(variadic.send(1, 2, uint32_t{3}, false, 1.25) == 21, "variadic fixed reply length");
    check(variadic.bytes == Bytes{0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0, 3, 0,
                                 0x3f, 0xf4, 0, 0, 0, 0, 0, 0}, "variadic fixed reply endian bytes");
    struct FailedCapture : Capture {
        int result = 0;
        int write_bytes(std::span<const std::byte>) override { return result; }
    } failed;
    failed.result = -1;
    check(failed.send(1, 2, uint32_t{3}) == -1 && !failed.closed(), "fixed reply error status");
    failed.result = 0;
    check(failed.send(1, 2, uint32_t{3}) == 0 && failed.closed(), "fixed reply closed status");

    // A transport can reuse its completion timestamp for duration accounting.
    // Both the explicit-time and legacy two-argument APIs retain byte totals.
    ut::RateTracker rates;
    rates.update_over_duration(300, std::chrono::seconds{3}, ut::RateTracker::clock::now());
    auto snapshot = rates.snapshot();
    check(snapshot.total_bytes == 300 && snapshot.window_bps == 480., "completion-time rate accounting");
    rates.update_over_duration(200, std::chrono::milliseconds{500});
    rates.update_over_duration(-1, std::chrono::seconds{1});
    rates.update_over_duration(10, std::chrono::seconds{0});
    snapshot = rates.snapshot();
    check(snapshot.total_bytes == 500 && snapshot.window_bps == 800., "legacy duration rate accounting");
}

template<class T>
void compare(T&& reply, std::span<const std::byte> first, std::span<const std::byte> second = {}) {
    Capture session;
    std::pmr::vector<unsigned char> packed;
    net::CommandBuilder builder;
    builder.reset_into(packed);
    builder.write_header(0xbeef, 0x1234);
    builder.push(reply);
    check(session.send(0xbeef, 0x1234, std::forward<T>(reply)) == static_cast<int>(packed.size()), "send return value");
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
        std::string(12000, 'z'), vector}, 1.25,
        std::complex<float>{2.5f, -3.0f}, std::array<uint8_t, 0>{}, uint16_t{0xabcd}};
    compare(std::move(nested), std::as_bytes(std::span{std::get<0>(std::get<2>(nested))}),
            std::as_bytes(std::span{std::get<2>(std::get<2>(nested))}));
    auto vectors = std::tuple{vector, uint8_t{19}, array};
    compare(std::move(vectors), std::as_bytes(std::span{std::get<0>(vectors)}),
            std::as_bytes(std::span{std::get<2>(vectors)}));
    Capture small;
    small.send(1, 2, std::tuple{uint32_t{3}, std::vector<uint8_t>(4095), uint16_t{4}});
    check(!small.segmented, "small payload entered scatter path");
    Capture many;
    auto lots = std::tuple{vector, vector, vector, vector, vector, vector, vector, vector,
                          vector, vector, vector, vector, vector, vector, vector, vector};
    many.send(1, 2, std::move(lots));
    check(!many.segmented, "descriptor limit did not fall back to packing");
    struct LegacyCapture : Capture {
        int write_segments(std::span<const std::span<const std::byte>> parts) override {
            return net::Session::write_segments(parts);
        }
    } legacy;
    std::pmr::vector<unsigned char> packed;
    net::CommandBuilder builder;
    builder.reset_into(packed); builder.write_header(1, 2); builder.push(vectors);
    legacy.send(1, 2, std::move(vectors));
    check(std::equal(packed.begin(), packed.end(), legacy.bytes.begin(), legacy.bytes.end()), "legacy transport fallback changed bytes");
}

struct MutatingCapture : Capture {
    std::vector<uint8_t>& source;
    explicit MutatingCapture(std::vector<uint8_t>& source_) : source(source_) {}
    int write_bytes(std::span<const std::byte> part) override {
        std::fill(source.begin(), source.end(), 0x22);
        return Capture::write_bytes(part);
    }
};

template<class T>
void check_snapshot(T&& reply, std::vector<uint8_t>& source, bool segmented = false) {
    std::pmr::vector<unsigned char> packed;
    net::CommandBuilder builder;
    builder.reset_into(packed); builder.write_header(2, 3); builder.push(reply);
    MutatingCapture session(source);
    session.send(2, 3, std::forward<T>(reply));
    check(session.segmented == segmented, "incorrect ownership classification");
    check(std::equal(packed.begin(), packed.end(), session.bytes.begin(), session.bytes.end()),
          "borrowed data changed after serialization");
    check(source.front() == 0x22, "mutation did not run");
}

void ownership() {
    using Vector = std::vector<uint8_t>;
    using Owned = std::tuple<uint32_t, Vector>;
    static_assert(net::mixed_reply_containers<Owned>() == 1);
    static_assert(net::mixed_reply_containers<Owned&>() == 0);
    static_assert(net::mixed_reply_containers<const Owned&>() == 0);
    static_assert(net::mixed_reply_containers<std::tuple<Owned&&, Vector>>() == 1);
    static_assert(net::mixed_reply_containers<std::tuple<std::span<const uint8_t>>>() == 0);

    Vector source(8192, 0x11);
    auto reset = [&] { std::fill(source.begin(), source.end(), 0x11); };
    check_snapshot(std::tuple{uint32_t{7}, std::span<const uint8_t>{source}}, source);
    reset();
    check_snapshot(std::tuple<uint32_t, const Vector&>{7, source}, source);
    reset();
    check_snapshot(std::tuple<uint32_t, Vector&&>{7, std::move(source)}, source);
    reset();
    auto referenced = std::tuple{uint32_t{7}, source};
    check_snapshot(referenced, std::get<1>(referenced));
    std::fill(std::get<1>(referenced).begin(), std::get<1>(referenced).end(), 0x11);
    check_snapshot(std::as_const(referenced), std::get<1>(referenced));
    reset();
    // Borrowed fields must still be packed when owned siblings use scatter/gather.
    auto inner = std::tuple{std::span<const uint8_t>{source}, uint16_t{19}};
    auto mixed = std::tuple<Vector, Vector&, Vector&&, decltype(inner)&>{source, source, std::move(source), inner};
    check_snapshot(std::move(mixed), source, true);
    reset();
    auto owned = std::tuple{uint32_t{7}, source};
    check_snapshot(std::move(owned), source, true);
    reset();
    const auto const_owned = std::tuple{uint32_t{7}, source};
    check_snapshot(std::move(const_owned), source, true);
}

void reference_returns() {
    using Reply = std::tuple<uint32_t, std::vector<uint8_t>>;
    struct Driver {
        Reply reply{7, std::vector<uint8_t>(8192, 0x11)};
        Reply& lvalue() { return reply; }
        const Reply& const_lvalue() const { return reply; }
        Reply&& rvalue() { return std::move(reply); }
        Reply owned() const { return reply; }
    } driver;
    struct Probe : net::SocketSession<net::TCP> {
        using net::Session::read_command;
        Driver& driver;
        Bytes received;
        bool segmented = false;
        Probe(int fd, Driver& driver_) : SocketSession(fd, 0), driver(driver_) {}
        int write_bytes(std::span<const std::byte> part) override {
            auto& source = std::get<1>(driver.reply);
            std::fill(source.begin(), source.end(), 0x22);
            const auto* p = reinterpret_cast<const unsigned char*>(part.data());
            received.insert(received.end(), p, p + part.size());
            return static_cast<int>(part.size());
        }
        int write_segments(std::span<const std::span<const std::byte>> parts) override {
            segmented = true;
            int n = 0;
            for (auto part : parts) n += write_bytes(part);
            return n;
        }
    };
    auto run = [&](auto method, bool owned) {
        std::fill(std::get<1>(driver.reply).begin(), std::get<1>(driver.reply).end(), 0x11);
        int fd[2];
        check(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0, "socketpair");
        Probe session(fd[0], driver);
        const std::array<unsigned char, 8> header{};
        check(::write(fd[1], header.data(), header.size()) == 8, "command write");
        net::Command cmd;
        check(session.read_command(cmd) == 8, "command read");
        check(cmd.op_invoke(driver, method) == 8208, "reply length");
        check(session.segmented == owned, "reference return treated as owned");
        check(std::all_of(session.received.begin() + 16, session.received.end(),
                         [](auto byte) { return byte == 0x11; }), "reference return lost its snapshot");
        close(fd[0]); close(fd[1]);
    };
    run(&Driver::lvalue, false);
    run(&Driver::const_lvalue, false);
    run(&Driver::rvalue, false);
    run(&Driver::owned, true);
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
    // Repeated valid spans exceed the packed WebSocket limit before any write.
    std::vector<std::byte> storage(net::WEBSOCK_SEND_BUF_LEN / 2);
    const std::array<std::span<const std::byte>, 2> parts{storage, storage};
    check(ws.send_parts(parts) == -1 && calls == 0, "WebSocket limit reached socket");
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
    if (websocket && size + prefix.size() + suffix.size() > net::WEBSOCK_SEND_BUF_LEN - 10) {
        check(result == -1 && calls == 0 && received.empty() && !ws.is_closed(),
              "oversized mixed WebSocket reply changed behavior");
        return;
    }
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
        else if (test == "fixed") fixed_replies();
        else if (test == "ownership") ownership();
        else if (test == "reference-returns") reference_returns();
        else if (test == "limits") limits();
        else if (test == "boundaries") {
            for (auto size : {0u, 115u, 116u, 65525u, 65526u, 262123u, 262124u, 262125u, 1048576u})
                for (bool ws : {false, true}) transport(ws, Mode::normal, size);
        } else if (test == "partial") {
            for (bool ws : {false, true}) for (auto m : {Mode::partial, Mode::interrupt})
                transport(ws, m, 262124);
        } else if (test == "failures") {
            for (bool ws : {false, true}) for (auto m : {Mode::failure, Mode::zero}) transport(ws, m, 4096);
        } else throw std::runtime_error("unknown case");
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
