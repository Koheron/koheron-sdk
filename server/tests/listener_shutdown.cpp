#include "server/network/listener_manager.hpp"
#include "server/runtime/executor.hpp"

#include <arpa/inet.h>
#include <sys/un.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
std::mutex gate_mutex;
std::condition_variable gate_changed;
bool hold_eof = false;
int eof_waiters = 0;
std::atomic<bool> returned{false};
std::atomic<int> active_reads{0};

void check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}

template<class Predicate>
void wait_for(Predicate predicate, const char* message) {
    const auto end = std::chrono::steady_clock::now() + 3s;
    while (!predicate()) {
        check(std::chrono::steady_clock::now() < end, message);
        std::this_thread::sleep_for(1ms);
    }
}

struct Executor : rt::IExecutor {
    int handle_app(net::Command&) override { return 0; }
};

int connect_tcp(unsigned port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    check(fd >= 0, "socket");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    check(::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "connect TCP");
    return fd;
}

int connect_unix() {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    check(fd >= 0, "socket UNIX");
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strcpy(address.sun_path, net::config::unix_socket_path);
    check(::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "connect UNIX");
    return fd;
}

std::size_t descriptors() {
    return std::distance(std::filesystem::directory_iterator("/proc/self/fd"),
                         std::filesystem::directory_iterator{});
}

void shutdown_active() {
    // Keep each worker inside the production read path after shutdown wakes it.
    // The listener must stay alive until every worker has finished its cleanup.
    const auto before = descriptors();
    std::vector<int> clients;
    {
        net::ListenerManager listener;
        check(listener.start() == 0, "start");
        wait_for([&] { return listener.is_ready(); }, "listener readiness");
        clients = {connect_tcp(net::config::tcp_port),
                   connect_tcp(net::config::websocket_port), connect_unix()};
        for (int fd : clients) check(::send(fd, "G", 1, MSG_NOSIGNAL) == 1, "partial request");
        wait_for([] { return services::require<net::SessionManager>().get_number_of_sessions() == 3; },
                 "sessions registered");
        wait_for([] { return active_reads == 3; }, "workers reading partial requests");
        {
            std::lock_guard lock(gate_mutex);
            hold_eof = true;
        }
        std::thread release([] {
            std::unique_lock lock(gate_mutex);
            check(gate_changed.wait_for(lock, 3s, [] { return eof_waiters == 3; }), "all workers reached EOF");
            lock.unlock();
            std::this_thread::sleep_for(30ms);
            const bool too_early = returned.load();
            lock.lock();
            hold_eof = false;
            gate_changed.notify_all();
            check(!too_early, "shutdown returned while session workers were still running");
        });
        listener.shutdown();
        returned = true;
        release.join();
        check(services::require<net::SessionManager>().get_number_of_sessions() == 0, "sessions cleaned");
    }
    for (int fd : clients) ::close(fd);
    check(descriptors() == before, "shutdown leaked file descriptors");
}

void connection_limit() {
    net::ListenerManager listener;
    check(listener.start() == 0, "start");
    wait_for([&] { return listener.is_ready(); }, "listener readiness");
    std::vector<int> clients;
    for (int i = 0; i < 48; ++i) clients.push_back(connect_tcp(net::config::tcp_port));
    std::this_thread::sleep_for(100ms);
    const auto count = services::require<net::SessionManager>().get_number_of_sessions();
    check(count > 0 && count <= net::config::tcp_worker_connections, "connection limit exceeded");
    for (int fd : clients) ::close(fd);
    listener.shutdown();
    check(services::require<net::SessionManager>().get_number_of_sessions() == 0, "sessions cleaned");
}
} // namespace

extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __wrap_read(int fd, void* buffer, size_t size) {
    ++active_reads;
    const auto result = __real_read(fd, buffer, size);
    --active_reads;
    if (result <= 0) {
        std::unique_lock lock(gate_mutex);
        if (hold_eof) {
            ++eof_waiters;
            gate_changed.notify_all();
            gate_changed.wait(lock, [] { return !hold_eof; });
        }
    }
    return result;
}

int main(int argc, char** argv) {
    check(argc == 2, "case required");
    rt::provide_executor<Executor>();
    if (std::strcmp(argv[1], "shutdown") == 0) shutdown_active();
    else if (std::strcmp(argv[1], "limit") == 0) connection_limit();
    else check(false, "unknown case");
}
