#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

// Deliberately returns driver-owned storage, including nested borrowed views.
struct LockingInstrument {
    inline static std::atomic<unsigned> constructions{0}, calls{0}, readers{0}, active{0}, max_active{0};
    uint32_t value = 7;
    std::vector<uint32_t> data = std::vector<uint32_t>(512 * 1024, 0x33333333);
    std::array<uint32_t, 4> array{1, 2, 3, 4};
    std::string text = "before";
    LockingInstrument() { ++constructions; std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    void set(uint32_t v) {
        ++calls;
        value = v;
        std::fill(data.begin(), data.end(), v);
        array.fill(v);
        text = "after mutation";
    }
    uint32_t get() const { return value; }
    void set_vector(const std::vector<uint32_t>& v) { ++calls; value = v.at(0); }
    const std::vector<uint32_t>& get_vector() const { ++readers; return data; }
    std::span<const uint32_t> get_span() const { ++readers; return data; }
    const std::array<uint32_t, 4>& get_array() const { return array; }
    std::span<const uint32_t, 4> get_fixed_span() const { return array; }
    auto get_views() const { return std::tuple{std::span<const uint32_t>{data}, std::string_view{text}, text.c_str()}; }
    uint32_t busy(uint32_t v) {
        const auto n = ++active;
        auto previous = max_active.load();
        while (previous < n && !max_active.compare_exchange_weak(previous, n)) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        --active;
        return v;
    }
};
