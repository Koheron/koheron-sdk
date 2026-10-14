#ifndef KOHERON_BIST_WORKLOAD_HPP
#define KOHERON_BIST_WORKLOAD_HPP
#include "measurements.hpp"
#include <atomic>
#include <barrier>
#include <pthread.h>
#include <sched.h>
#include <thread>

namespace bist
{
struct Profile
{
    unsigned id = 0;
    double dma_seconds() const { return id == 1 ? 2.0 : .5; }
    double rpc_seconds() const { return id == 1 ? 1.0 : .3; }
    double window_seconds() const { return id == 1 ? 2.0 : .5; }
    size_t uart_bytes() const { return id == 1 ? 131072 : 32768; }
    const char* name() const { return id == 1 ? "benchmark" : id == 2 ? "diagnostic" : "health"; }
};

class StartGate
{
    struct Release
    {
        StartGate* owner;
        void operator()() noexcept
        {
            owner->epoch = Clock::now();
            owner->released = true;
        }
    };

  public:
    explicit StartGate(unsigned participants) : gate(participants, Release{this}) {}
    void wait() { gate.arrive_and_wait(); }
    void drop()
    {
        ++dropped;
        gate.arrive_and_drop();
    }
    void await_release() const
    {
        while (!released)
            std::this_thread::yield();
    }
    Clock::time_point start() const { return epoch; }
    unsigned failures() const { return dropped; }

  private:
    std::barrier<Release> gate;
    Clock::time_point epoch;
    std::atomic<unsigned> dropped{0};
    std::atomic<bool> released{false};
};

struct CombinedLoad
{
    StartGate ready{5}; // CPU, DMA, RPC client and both UART workers.
    std::atomic<bool> serial_done{false}, rpc_done{false};
    double window_seconds = .5;
    double cpu_start_us = 0, dma_start_us = 0, rpc_start_us = 0;
    int cpu_affinity_error = 0, dma_affinity_error = 0, rpc_affinity_error = 0;
    bool finished() const
    {
        return serial_done && rpc_done && seconds(ready.start()) >= window_seconds;
    }
    Clock::time_point window_end() const
    {
        return ready.start() + std::chrono::duration_cast<Clock::duration>(
                                   std::chrono::duration<double>(window_seconds));
    }
};

inline int pin_worker(unsigned cpu)
{
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(cpu, &cpus);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpus), &cpus);
}

} // namespace bist
#endif
