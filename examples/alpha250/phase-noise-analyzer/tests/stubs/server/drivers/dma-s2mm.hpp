#pragma once
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include "server/hardware/memory_manager.hpp"
class DmaS2MM {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<bool> completions;
    unsigned waits = 0;
    bool cancelled = false;
public:
    template<int, uint32_t, class> void start_transfer() {
        auto& ctl = hw::get_memory<mem::control>();
        hw::captured_channel = (ctl.words[reg::cordic / 4].load() >> 4) & 1;
        hw::captured_precision = ctl.words[reg::phase_precision / 4].load();
        hw::captured_rate = ctl.words[reg::cic_rate / 4].load();
        const uint32_t word = 2 * hw::captured_channel;
        const uint64_t increment = uint64_t(ctl.words[word].load()) | uint64_t(ctl.words[word + 1].load()) << 32;
        hw::captured_lo = static_cast<double>(increment) * hw::test_sampling_frequency / (uint64_t{1} << 48);
        hw::dma_in_flight.store(true);
    }
    template<class Duration> bool wait_for_transfer_checked(Duration) {
        std::unique_lock lock(mutex);
        ++waits;
        cv.notify_all();
        cv.wait(lock, [&] { return cancelled || !completions.empty(); });
        hw::dma_in_flight.store(false);
        if (cancelled) return false;
        hw::get_memory<mem::status>().words[reg::phase_packet / 4].store(
            (hw::injected_precision.load() <= 15 ? hw::injected_precision.load() : hw::captured_precision) |
            hw::injected_packet_flags.load());
        const bool result = completions.front();
        completions.pop_front();
        return result;
    }
    void wait_until(unsigned target) {
        std::unique_lock lock(mutex);
        const bool ready = cv.wait_for(lock, std::chrono::seconds(5), [&] { return waits >= target; });
        assert(ready);
    }
    void complete(bool success) {
        std::lock_guard lock(mutex);
        completions.push_back(success);
        cv.notify_all();
    }
    void cancel() {
        std::lock_guard lock(mutex);
        cancelled = true;
        cv.notify_all();
    }
};
