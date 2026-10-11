#pragma once
#include <mutex>
struct VNAClock {
    static constexpr double rate = 125000000.0;
    static std::mutex& mutex() { static std::mutex clock_mutex; return clock_mutex; }
    static bool matches() { return true; } // Fixed board oscillator, no software clock control.
    static void ensure() {}
};
