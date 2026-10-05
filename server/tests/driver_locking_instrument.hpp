#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <complex>
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
    uint32_t set_scalars(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
        ++calls;
        return value = a + 2 * b + 3 * c + 4 * d;
    }
    bool fixed_types(uint16_t a, int32_t b, float c, bool d, double e,
                     const std::array<uint32_t, 3>& words, std::complex<double> z, uint8_t f) {
        ++calls;
        return a == 0xbeef && b == -123456 && c == 3.5f && d && e == -2.25 &&
               words == std::array<uint32_t, 3>{0xabcd1234, 0x80007fff, 0x0a0b0c0d} &&
               z == std::complex<double>{1.25, -3.0} && f == 42;
    }
    bool mixed_types(uint16_t prefix, const std::vector<uint32_t>& words,
                     uint32_t suffix, const std::string& text) {
        ++calls;
        return prefix == 0xbeef && words == std::vector<uint32_t>{1, 0x12345678} &&
               suffix == 0xabcdef01 && text == "hello";
    }
    uint32_t empty_arrays(const std::array<uint32_t, 0>&, const std::array<uint32_t, 0>&) {
        ++calls;
        return 42;
    }
    bool large_arrays(const std::array<uint32_t, 8192>& a, uint32_t marker,
                      const std::array<uint32_t, 8192>& b) {
        ++calls;
        if (marker != 0xabcdef01) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] != i * 3 || b[i] != i * 7) return false;
        }
        return true;
    }
    uint32_t busy(uint32_t v) {
        const auto n = ++active;
        auto previous = max_active.load();
        while (previous < n && !max_active.compare_exchange_weak(previous, n)) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        --active;
        return v;
    }
};
