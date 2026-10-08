// Real sessions, Executor, DriverManager and socket backpressure; no board needed.
#include "server/executor/executor.hpp"
#include "server/network/socket_session.hpp"
#include "server/runtime/driver_manager.hpp"
#include "driver_locking_instrument.hpp"
#include <barrier>
#include <functional>
#include <poll.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <iostream>

using namespace std::chrono_literals;
using Bytes = std::vector<unsigned char>;

// Count real server read calls for a fully queued body. Client reads use recv().
std::atomic<int> measured_fd{-1};
std::atomic<unsigned> reads4{0}, reads16{0};
extern "C" ssize_t __real_read(int fd, void* buffer, size_t count);
extern "C" ssize_t __wrap_read(int fd, void* buffer, size_t count) {
    if (fd == measured_fd.load()) {
        if (count == 4) ++reads4;
        if (count == 16) ++reads16;
    }
    return __real_read(fd, buffer, count);
}

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
template<class F> void until(F predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!predicate()) {
        check(std::chrono::steady_clock::now() < deadline, "Timed out waiting for server");
        std::this_thread::sleep_for(1ms);
    }
}
void send_all(int fd, const Bytes& bytes) {
    size_t done = 0;
    while (done < bytes.size()) {
        auto n = ::send(fd, bytes.data() + done, bytes.size() - done, MSG_NOSIGNAL);
        check(n > 0, "Client send failed");
        done += n;
    }
}
Bytes read_bytes(int fd, size_t length) {
    Bytes bytes(length);
    size_t done = 0;
    while (done < length) {
        pollfd ready{fd, POLLIN, 0};
        check(::poll(&ready, 1, 2000) == 1, "Response blocked by another client");
        auto n = ::recv(fd, bytes.data() + done, length - done, 0);
        check(n > 0, "Response was truncated");
        done += n;
    }
    return bytes;
}
void append_be(Bytes& bytes, uint64_t value, size_t width) {
    for (size_t i = width; i > 0; --i) bytes.push_back(value >> (8 * (i - 1)));
}
Bytes command(uint16_t operation) {
    Bytes bytes(4, 0);
    append_be(bytes, 2, 2);
    append_be(bytes, operation, 2);
    return bytes;
}
Bytes scalar_command(uint16_t operation, uint32_t value) {
    auto bytes = command(operation); append_be(bytes, value, 4); return bytes;
}
Bytes scalar_response(uint16_t operation, uint32_t value) { return scalar_command(operation, value); }

template<int Kind> struct Connection {
    int client = -1, server = -1;
    std::unique_ptr<net::SocketSession<Kind>> session;
    std::thread worker;
    explicit Connection(unsigned id, bool autostart = true) {
        if constexpr (Kind == net::UNIX) {
            int sockets[2]; check(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0, "socketpair");
            server = sockets[0]; client = sockets[1];
        } else {
            int listener = ::socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            check(::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind");
            socklen_t length = sizeof(address);
            check(::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0, "getsockname");
            check(::listen(listener, 1) == 0, "listen");
            client = ::socket(AF_INET, SOCK_STREAM, 0);
            check(::connect(client, reinterpret_cast<sockaddr*>(&address), length) == 0, "connect");
            server = ::accept(listener, nullptr, nullptr); ::close(listener);
            check(server >= 0, "accept");
        }
        const int buffer_size = 4096;
        check(::setsockopt(server, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size)) == 0, "send buffer");
        session = std::make_unique<net::SocketSession<Kind>>(server, id);
        if (autostart) start();
        if constexpr (Kind == net::WEBSOCK) {
            const std::string request = "GET / HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
            send_all(client, Bytes(request.begin(), request.end()));
            std::string response;
            while (!response.ends_with("\r\n\r\n")) response += char(read_bytes(client, 1)[0]);
            check(response.find("101") != std::string::npos, "WebSocket handshake failed");
        }
    }
    void start() { worker = std::thread([this] { session->run(); }); }
    ~Connection() {
        ::shutdown(client, SHUT_RDWR);
        ::shutdown(server, SHUT_RDWR); // Wake blocked writes even with unread TCP data.
        if (worker.joinable()) worker.join();
        session->shutdown();
        ::close(client);
    }
    void send(const Bytes& bytes) {
        if constexpr (Kind == net::WEBSOCK) {
            check(bytes.size() < 126, "Test frame too large");
            Bytes frame{0x82, static_cast<unsigned char>(0x80 | bytes.size()), 0, 0, 0, 0};
            frame.insert(frame.end(), bytes.begin(), bytes.end()); send_all(client, frame);
        } else send_all(client, bytes);
    }
    Bytes response(size_t length) {
        if constexpr (Kind == net::WEBSOCK) {
            Bytes result;
            bool first = true;
            while (true) {
                auto header = read_bytes(client, 2);
                check((header[0] & 0x0f) == (first ? 2 : 0), "Unexpected WebSocket opcode");
                check(!(header[1] & 0x80), "Masked server frame");
                uint64_t size = header[1] & 0x7f;
                if (size >= 126) {
                    auto extended = read_bytes(client, size == 126 ? 2 : 8);
                    size = 0; for (auto byte : extended) size = (size << 8) | byte;
                }
                check(size <= length - result.size(), "Unexpected frame size");
                auto payload = read_bytes(client, size); result.insert(result.end(), payload.begin(), payload.end());
                if (header[0] & 0x80) break;
                first = false;
            }
            check(result.size() == length, "Unexpected response length");
            return result;
        } else return read_bytes(client, length);
    }
    void consumed() {
        until([this] { int queued = -1; return ::ioctl(server, FIONREAD, &queued) == 0 && queued == 0; });
    }
};

template<int Kind> void stalled_input(bool dynamic) {
    Connection<Kind> healthy(2), slow(1);
    // Warm the wrapper first so this test isolates lock scope from initialization.
    healthy.send(command(1)); check(healthy.response(12) == scalar_response(1, 7), "Initial value");
    auto partial = command(dynamic ? 2 : 0);
    if (dynamic) {
        append_be(partial, 8, 4);
        const uint32_t first = 42;
        const auto* p = reinterpret_cast<const unsigned char*>(&first); partial.insert(partial.end(), p, p + 4);
    } else { partial.push_back(0); partial.push_back(0); }
    slow.send(partial); slow.consumed();
    healthy.send(command(1)); check(healthy.response(12) == scalar_response(1, 7), "Partial request executed");
    check(LockingInstrument::calls == 0, "Driver called before arguments finished");
    if (dynamic) {
        const uint32_t last = 0; const auto* p = reinterpret_cast<const unsigned char*>(&last); slow.send(Bytes(p, p + 4));
    } else slow.send(Bytes{0, 42});
    slow.send(command(1)); check(slow.response(12) == scalar_response(1, 42), "Completed request lost");
    check(LockingInstrument::calls == 1, "Wrong invocation count");
}

template<int Kind> void stalled_output(uint16_t operation, bool disconnect = false) {
    Connection<Kind> healthy(2), slow(1);
    slow.send(command(operation));
    until([] { return LockingInstrument::readers.load() == 1; });
    // The first client does not read. Its 2 MiB reply cannot fit the socket buffer.
    healthy.send(scalar_command(0, 0x44444444)); healthy.consumed(); healthy.send(command(1));
    pollfd ready{healthy.client, POLLIN, 0};
    check(::poll(&ready, 1, 100) == 0, "Driver storage unlocked before its reply finished");
    check(LockingInstrument::calls == 0, "Borrowed storage mutated during transmission");
    if (disconnect) {
        ::shutdown(slow.client, SHUT_RDWR);
        ::shutdown(slow.server, SHUT_RDWR);
        check(healthy.response(12) == scalar_response(1, 0x44444444), "Disconnected writer kept the driver locked");
        return;
    }
    Bytes expected = command(operation);
    if (operation == 14) append_be(expected, 0xbeef, 2);
    append_be(expected, 512 * 1024 * sizeof(uint32_t), operation == 14 ? 4 : sizeof(size_t));
    expected.resize(expected.size() + 512 * 1024 * sizeof(uint32_t), 0x33);
    if (operation == 14) append_be(expected, 0xabcdef01, 4);
    check(slow.response(expected.size()) == expected, "Borrowed response changed during transmission");
    check(healthy.response(12) == scalar_response(1, 0x44444444), "Driver stayed locked after its reply finished");
    slow.send(command(1)); check(slow.response(12) == scalar_response(1, 0x44444444), "Connection lost after large reply");
}

void concurrent_first_commands() {
    constexpr unsigned count = 12;
    std::vector<std::unique_ptr<Connection<net::TCP>>> connections;
    for (unsigned i = 0; i < count; ++i) connections.push_back(std::make_unique<Connection<net::TCP>>(i));
    std::barrier start(count);
    std::vector<std::thread> clients;
    std::atomic<unsigned> failures{0};
    for (unsigned i = 0; i < count; ++i) clients.emplace_back([&, i] {
        try {
            start.arrive_and_wait();
            connections[i]->send(scalar_command(8, i));
            check(connections[i]->response(12) == scalar_response(8, i), "Busy reply wrong");
        } catch (...) { ++failures; }
    });
    for (auto& client : clients) client.join();
    check(failures == 0, "Concurrent request failed");
    check(LockingInstrument::constructions == 1, "Driver constructed twice");
    check(LockingInstrument::max_active == 1, "Driver methods executed concurrently");
}

void truncated_input() {
    Connection<net::TCP> broken(1), healthy(2);
    broken.send(Bytes{0,0,0,0,0,2,0,0,0,0}); broken.consumed();
    ::shutdown(broken.client, SHUT_WR);
    healthy.send(command(1)); check(healthy.response(12) == scalar_response(1, 7), "Disconnected client blocked peer");
    check(LockingInstrument::calls == 0, "Truncated input invoked driver");
}

void concurrent_manager_access() {
    // Bypass the adapter's call_once, as constructors of different drivers do
    // when resolving a shared dependency through DriverManager.
    constexpr unsigned count = 12;
    std::barrier start(count);
    std::array<LockingInstrument*, count> instances{};
    std::vector<std::thread> callers;
    std::atomic<unsigned> failures{0};
    auto& manager = services::require<rt::DriverManager>();
    for (unsigned i = 0; i < count; ++i) callers.emplace_back([&, i] {
        start.arrive_and_wait();
        auto& driver = manager.get<LockingInstrument>();
        instances[i] = &driver;
        if (driver.value != 7 || driver.data.size() != 512 * 1024 ||
            driver.data.front() != 0x33333333 || driver.text != "before") ++failures;
    });
    for (auto& caller : callers) caller.join();
    check(failures == 0, "Driver published before construction finished");
    check(LockingInstrument::constructions == 1, "Shared driver constructed twice");
    for (auto* instance : instances) check(instance == instances[0], "Different shared driver instances");
}

Bytes four_scalars() {
    auto bytes = command(9);
    for (uint32_t value : {1, 2, 3, 4}) append_be(bytes, value, 4);
    return bytes;
}
template<int Kind> void batched_read_calls() {
    Connection<Kind> client(1, false);
    // Preload the complete command to exclude network fragmentation from the count.
    client.send(four_scalars());
    measured_fd = client.server;
    client.start();
    check(client.response(12) == scalar_response(9, 30), "Scalar argument order changed");
    measured_fd = -1;
    std::cout << "body reads: 16-byte=" << reads16 << ", 4-byte=" << reads4 << '\n';
    check(reads16 == 1 && reads4 == 0, "Fixed arguments were read separately");
    check(LockingInstrument::calls == 1, "Scalar command invocation count");
}
Bytes fixed_command() {
    auto bytes = command(10);
    append_be(bytes, 0xbeef, 2);
    append_be(bytes, static_cast<uint32_t>(-123456), 4);
    append_be(bytes, 0x40600000, 4); // 3.5f
    bytes.push_back(1);
    append_be(bytes, 0xc002000000000000ULL, 8); // -2.25
    const std::array<uint32_t, 3> words{0xabcd1234, 0x80007fff, 0x0a0b0c0d};
    const auto* raw = reinterpret_cast<const unsigned char*>(words.data());
    bytes.insert(bytes.end(), raw, raw + sizeof(words));
    append_be(bytes, 0x3ff4000000000000ULL, 8); // complex real = 1.25
    append_be(bytes, 0xc008000000000000ULL, 8); // complex imaginary = -3.0
    bytes.push_back(42);
    return bytes;
}
Bytes bool_response(uint16_t operation) { auto bytes = command(operation); bytes.push_back(1); return bytes; }
template<int Kind> void fixed_wire_format() {
    Connection<Kind> client(1);
    client.send(fixed_command()); client.send(command(1));
    check(client.response(9) == bool_response(10), "Fixed scalar/array wire format changed");
    check(client.response(12) == scalar_response(1, 7), "Next command header consumed as arguments");
    check(LockingInstrument::calls == 1, "Fixed command invocation count");
}
template<int Kind> void partial_batch() {
    Connection<Kind> healthy(2), slow(1);
    auto bytes = four_scalars();
    slow.send(Bytes(bytes.begin(), bytes.begin() + 14)); // split the second argument
    slow.consumed();
    healthy.send(command(1));
    check(healthy.response(12) == scalar_response(1, 7), "Partial batch held driver lock");
    check(LockingInstrument::calls == 0, "Partial batch invoked driver");
    slow.send(Bytes(bytes.begin() + 14, bytes.end()));
    check(slow.response(12) == scalar_response(9, 30), "Fragmented batch decoded incorrectly");
    check(LockingInstrument::calls == 1, "Fragmented batch invocation count");
}
template<int Kind> void truncated_batch() {
    Connection<Kind> broken(1);
    auto bytes = four_scalars(); bytes.resize(bytes.size() - 1);
    broken.send(bytes);
    if constexpr (Kind != net::WEBSOCK) ::shutdown(broken.client, SHUT_WR);
    else {
        // A malformed frame must not advance the next frame's argument cursor.
        broken.send(command(1));
        check(broken.response(12) == scalar_response(1, 7), "Malformed frame affected next command");
    }
    Connection<net::TCP> healthy(2);
    healthy.send(command(1));
    check(healthy.response(12) == scalar_response(1, 7), "Truncated batch mutated driver");
    check(LockingInstrument::calls == 0, "Truncated batch invoked driver");
}
template<int Kind> void mixed_arguments() {
    Connection<Kind> client(1);
    auto bytes = command(11);
    append_be(bytes, 0xbeef, 2);
    const std::array<uint32_t, 2> words{1, 0x12345678};
    append_be(bytes, sizeof(words), 4);
    auto* raw = reinterpret_cast<const unsigned char*>(words.data());
    bytes.insert(bytes.end(), raw, raw + sizeof(words));
    append_be(bytes, 0xabcdef01, 4);
    append_be(bytes, 5, 4);
    bytes.insert(bytes.end(), {'h', 'e', 'l', 'l', 'o'});
    client.send(bytes);
    check(client.response(9) == bool_response(11), "Mixed dynamic argument decoding changed");
    client.send(command(1));
    check(client.response(12) == scalar_response(1, 7), "Mixed request consumed next header");
}
void empty_fixed_arrays() {
    Connection<net::UNIX> client(1);
    client.send(command(12)); client.send(command(1));
    check(client.response(12) == scalar_response(12, 42), "Zero-byte batch rejected");
    check(client.response(12) == scalar_response(1, 7), "Zero-byte batch read the next header");
}
void large_fixed_arrays() {
    Connection<net::TCP> client(1);
    std::array<uint32_t, 8192> a{}, b{};
    for (size_t i = 0; i < a.size(); ++i) { a[i] = i * 3; b[i] = i * 7; }
    auto bytes = command(13);
    auto append_raw = [&](const auto& array) {
        const auto* raw = reinterpret_cast<const unsigned char*>(array.data());
        bytes.insert(bytes.end(), raw, raw + sizeof(array));
    };
    append_raw(a); append_be(bytes, 0xabcdef01, 4); append_raw(b);
    client.send(bytes);
    check(client.response(9) == bool_response(13), "Large fixed-array batch decoded incorrectly");
    check(LockingInstrument::calls == 1, "Large fixed-array invocation count");
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Expected test case");
        services::provide<rt::DriverManager>();
        services::provide<rt::IExecutor>(std::make_shared<koheron::Executor>());
        const std::string test = argv[1];
        if (test == "tcp-input") stalled_input<net::TCP>(false);
        else if (test == "unix-input") stalled_input<net::UNIX>(false);
        else if (test == "dynamic-input") stalled_input<net::TCP>(true);
        else if (test == "tcp-output") stalled_output<net::TCP>(3);
        else if (test == "span-output") stalled_output<net::TCP>(4);
        else if (test == "ws-output") stalled_output<net::WEBSOCK>(3);
        else if (test == "disconnect-output") stalled_output<net::TCP>(3, true);
        else if (test == "concurrent-first") concurrent_first_commands();
        else if (test == "truncated-input") truncated_input();
        else if (test == "manager-publication") concurrent_manager_access();
        else if (test == "tcp-read-calls") batched_read_calls<net::TCP>();
        else if (test == "unix-read-calls") batched_read_calls<net::UNIX>();
        else if (test == "fixed-tcp") fixed_wire_format<net::TCP>();
        else if (test == "fixed-ws") fixed_wire_format<net::WEBSOCK>();
        else if (test == "partial-tcp-batch") partial_batch<net::TCP>();
        else if (test == "partial-unix-batch") partial_batch<net::UNIX>();
        else if (test == "truncated-tcp-batch") truncated_batch<net::TCP>();
        else if (test == "truncated-ws-batch") truncated_batch<net::WEBSOCK>();
        else if (test == "mixed-tcp") mixed_arguments<net::TCP>();
        else if (test == "mixed-ws") mixed_arguments<net::WEBSOCK>();
        else if (test == "empty-arrays") empty_fixed_arrays();
        else if (test == "large-arrays") large_fixed_arrays();
        else if (test == "mixed-tcp-output") stalled_output<net::TCP>(14);
        else if (test == "mixed-unix-output") stalled_output<net::UNIX>(14);
        else if (test == "mixed-ws-output") stalled_output<net::WEBSOCK>(14);
        else if (test == "mixed-disconnect-output") stalled_output<net::TCP>(14, true);
        else throw std::runtime_error("Unknown test case");
        std::cout << "PASS " << test << '\n';
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
