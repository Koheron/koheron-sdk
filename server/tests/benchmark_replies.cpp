// Standalone CPU benchmark: production packing/accounting, no socket I/O.
// The opaque send hooks keep byte construction observable to the optimizer.
#include "server/network/session.hpp"
#include <chrono>
#include <iostream>
#include <algorithm>
struct Sink final : net::Session {
    Sink() : Session(net::TCP) {}
    void shutdown() override {}
    int init_socket() override { return 0; }
    int exit_socket() override { return 0; }
    int read_command(net::Command&) override { return 0; }
    [[gnu::noinline]] int write_bytes(std::span<const std::byte> bytes) override {
        asm volatile("" : : "r"(bytes.data()), "r"(bytes.size()) : "memory");
        return static_cast<int>(bytes.size());
    }
    [[gnu::noinline]] int send_iov(std::span<const std::byte> a, std::span<const std::byte> b, int) override {
        asm volatile("" : : "r"(a.data()), "r"(a.size()), "r"(b.data()), "r"(b.size()) : "memory");
        return static_cast<int>(a.size() + b.size());
    }
};
template<class T> void measure(const char* name, T reply) {
    Sink sink;
    std::array<double, 15> times{};
    for (auto& time : times) {
        const auto start = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < 100000; ++i) sink.send(3,4,reply);
        time = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / 100000;
    }
    std::sort(times.begin(),times.end());
    std::cout << name << " " << times[times.size() / 2] << " ns\n";
}
int main() {
    measure("array", std::array<uint32_t,1024>{});
    measure("vector", std::vector<uint32_t>(1024));
    measure("scalar", uint32_t{123});
    measure("status", std::tuple{uint32_t{4},false,1.2,3.4,5.6,uint64_t{7},8.9,float{10.1}});
    measure("nested", std::tuple{std::tuple{uint32_t{4},false,1.2,3.4},std::tuple{5.6,uint64_t{7},8.9,float{10.1}}});
}
