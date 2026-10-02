#pragma once
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
class DmaS2MM {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<bool> completions;
    unsigned waits = 0;
    bool cancelled = false;
public:
    template<int, uint32_t, class> void start_transfer() {}
    template<class Duration> bool wait_for_transfer_checked(Duration) {
        std::unique_lock lock(mutex);
        ++waits;
        cv.notify_all();
        cv.wait(lock, [&] { return cancelled || !completions.empty(); });
        if (cancelled) return false;
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
