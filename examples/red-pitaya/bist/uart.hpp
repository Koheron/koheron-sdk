#ifndef KOHERON_BIST_UART_HPP
#define KOHERON_BIST_UART_HPP
#include "hardware.hpp"
#include <pthread.h>
#include "workload.hpp"
#include <sched.h>
#include <cstring>
namespace bist
{
struct SerialResult
{
    uint64_t errors = 0;
    unsigned interrupts = 0, timeouts = 0;
    size_t received = 0, transmitted = 0;
    double seconds = 0;
    uint32_t frame_cycles = 0;
    std::vector<std::string> diagnostics;
    std::vector<double> roundtrip_us;
    int scheduling_error = 0;
    LatencyHistogram observation_us, window_observation_us;
    size_t window_received = 0;
    double start_offset_us = 0;
};
template <MemID Id> class SerialEndpoint
{
  public:
    explicit SerialEndpoint(std::atomic<bool>& cancelled_)
        : port(hw::get_memory<Id>()), cancelled(cancelled_)
    {
    }
    bool open() { return irq.open() >= 0; }
    void reset()
    {
        port.write_reg(8, 0U);
        port.write_reg(12, 1U);
        barrier();
    }
    bool idle_timeout()
    {
        reset();
        return irq.arm_irq() && irq.wait_for_irq(std::chrono::milliseconds(5)) == 0;
    }
    // Each endpoint owns its UIO FD and runs on exactly one worker thread.
    // Optional response gating implements a request/reply exchange through PL.
    SerialResult exchange(size_t bytes, unsigned endpoint, bool request_reply = false,
                          StartGate* ready = nullptr, CombinedLoad* load = nullptr)
    {
        SerialResult result;
        if (request_reply)
            result.roundtrip_us.reserve(bytes / 32);
        // A 64-byte RX FIFO provides 1389 us of service time at this baud.
        // Keep IRQ workers ahead of the normal CPU/DMA load threads. Each
        // worker blocks on UIO between causes and has a fixed five-second bound.
        cpu_set_t cpus;
        CPU_ZERO(&cpus);
        CPU_SET(endpoint, &cpus);
        const int affinity_error = pthread_setaffinity_np(pthread_self(), sizeof(cpus), &cpus);
        if (affinity_error != 0)
        {
            ++result.errors;
            result.diagnostics.push_back(std::string("UART worker CPU affinity: ") +
                                         std::strerror(affinity_error));
        }
        sched_param scheduling{};
        scheduling.sched_priority = 1;
        result.scheduling_error = pthread_setschedparam(pthread_self(), SCHED_FIFO, &scheduling);
        if (result.scheduling_error != 0)
        {
            ++result.errors;
            result.diagnostics.push_back(std::string("SCHED_FIFO priority 1: ") +
                                         std::strerror(result.scheduling_error));
        }
        // Wait for the peer's affinity and real-time policy before allowing
        // either transmitter to fill its FIFO. Synchronizing thread entry alone
        // leaves a normal-priority peer vulnerable during CPU load.
        if (ready != nullptr)
            ready->wait();
        Clock::time_point frame_start;
        const auto start = Clock::now(), deadline = start + std::chrono::seconds(5);
        if (load)
            result.start_offset_us = bist::seconds(load->ready.start()) * 1e6;
        while (!cancelled && Clock::now() < deadline)
        {
            uint32_t status = port.read_reg(4);
            unsigned pending = status & 255;
            const bool in_window = load && Clock::now() <= load->window_end();
            while (pending-- != 0)
            {
                const auto value = static_cast<uint8_t>(port.read_reg(0));
                if (result.received >= bytes ||
                    value != serial_pattern(result.received, 1 - endpoint))
                {
                    ++result.errors;
                    if (result.diagnostics.size() < 8)
                        result.diagnostics.push_back(
                            "sequence=" + std::to_string(result.received) + " expected=" +
                            std::to_string(serial_pattern(result.received, 1 - endpoint)) +
                            " observed=" + std::to_string(value));
                }
                ++result.received;
                if (in_window)
                    ++result.window_received;
                if (request_reply && endpoint == 0 && result.received % 32 == 0)
                    result.roundtrip_us.push_back(bist::seconds(frame_start) * 1e6);
            }
            unsigned queued = (port.read_reg(4) >> 8) & 255;
            // Request/reply uses one outstanding 32-byte frame. A sends the
            // request; B must receive all 32 bytes before sending its reply.
            const size_t limit =
                request_reply
                    ? std::min(bytes, (result.received / 32 + (endpoint == 0 ? 1 : 0)) * 32)
                    : bytes;
            while (result.transmitted < limit && queued < prm::uart_fifo_bytes)
            {
                if (request_reply && endpoint == 0 && result.transmitted % 32 == 0)
                    frame_start = Clock::now();
                port.write_reg(0, uint32_t(serial_pattern(result.transmitted, endpoint)));
                ++result.transmitted;
                ++queued;
            }
            status = port.read_reg(4);
            if (result.received >= bytes && result.transmitted == bytes && (status & 0x2ff00U) == 0)
                break;
            // Finish the last stop bit without an artificial poll timeout.
            if (result.received >= bytes && result.transmitted == bytes)
            {
                std::this_thread::yield();
                continue;
            }
            // TX demand interrupts are only enabled for unsent work.
            port.write_reg(8, 1U | (result.transmitted < limit ? 2U : 0U));
            barrier();
            // RX asserts on every byte. A fixed service pause lets normal
            // workers run even when AXI service costs exceed the byte period.
            // 100 us is well below the 1389 us FIFO service budget.
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            if (!irq.arm_irq())
            {
                ++result.errors;
                break;
            }
            const int observed = irq.wait_for_irq(std::chrono::milliseconds(10));
            if (observed < 0)
            {
                ++result.errors;
                break;
            }
            if (observed > 0)
            {
                ++result.interrupts;
                // Time from the PL's level-IRQ assertion to this register read.
                // Includes scheduler, UIO delivery and AXI read overhead.
                const uint32_t asserted = port.read_reg(36), now = port.read_reg(32);
                const double latency = double(now - asserted) / 125.0;
                result.observation_us.add(latency);
                if (load && Clock::now() <= load->window_end())
                    result.window_observation_us.add(latency);
            }
            else
                ++result.timeouts;
            // Loop services RX/TX causes before its next arm_irq().
        }
        port.write_reg(8, 0U);
        result.seconds = bist::seconds(start);
        result.frame_cycles = port.read_reg(28);
        result.errors += port.read_reg(24);
        if (result.received != bytes || result.transmitted != bytes)
            ++result.errors;
        return result;
    }

  private:
    hw::Memory<Id>& port;
    std::atomic<bool>& cancelled;
    Uio<Id> irq;
};
} // namespace bist
#endif
