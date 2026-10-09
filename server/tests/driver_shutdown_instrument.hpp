#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include <cassert>
#include <chrono>
#include "server/runtime/driver_manager.hpp"

inline std::vector<int> destroyed;
inline std::atomic<unsigned> worker_reads{0};
struct Bus {
    std::atomic<bool> alive{true};
    ~Bus() { alive = false; destroyed.push_back(0); }
};
struct Dependency {
    Bus& bus = services::require<Bus>();
    ~Dependency() { assert(bus.alive); destroyed.push_back(1); }
    void read() { assert(bus.alive); ++worker_reads; }
};
struct Acquisition {
    Dependency& dependency = rt::get_driver<Dependency>();
    std::atomic<bool> running{true};
    std::thread worker{[this] {
        while (running) {
            // The registry must remain available while workers stop.
            rt::get_driver<Dependency>().read();
            std::this_thread::yield();
        }
    }};
    ~Acquisition() {
        running = false;
        worker.join();
        dependency.read();
        destroyed.push_back(2);
    }
};
struct Unused {
    ~Unused() { std::abort(); }
};
