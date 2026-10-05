#pragma once
#include <array>
#include <cstdint>
#include <condition_variable>
#include <mutex>
class Ltc2157 {
    std::mutex mutex;
    std::condition_variable condition;
    bool block = false, blocked = false;
public:
    double get_input_voltage_range(uint32_t) {
        std::unique_lock lock(mutex);
        if (block) {
            blocked = true;
            condition.notify_all();
            condition.wait(lock, [&] { return !block; });
        }
        return 1.0;
    }
    void block_conversion() { std::lock_guard lock(mutex); block = true; blocked = false; }
    void wait_for_conversion() {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return blocked; });
    }
    void release_conversion() {
        std::lock_guard lock(mutex); block = false; condition.notify_all();
    }
    template<class T> auto tf_polynomial(uint32_t) { return std::array<T, 1>{T{1}}; }
};
