#ifndef KOHERON_BIST_PACING_HPP
#define KOHERON_BIST_PACING_HPP
#include "workload.hpp"
#include "upload.hpp"

namespace bist
{
// Absolute releases with one operation outstanding. Expired slots are skipped,
// never replayed in a catch-up burst. Counts include the entire requested
// window.
class Schedule
{
  public:
    Schedule(Clock::time_point epoch_, Clock::duration period_, uint64_t slots_)
        : epoch(epoch_), period(period_), slots(slots_)
    {
    }
    Clock::time_point release() const { return epoch + period * next; }
    bool done() const { return next == slots; }
    void skip_expired(Clock::time_point now)
    {
        if (now <= release() || done())
            return;
        const auto expired = std::min(slots, uint64_t((now - epoch) / period));
        if (expired > next)
        {
            missed += expired - next;
            next = expired;
        }
    }
    void advance() { ++next; }
    uint64_t missed = 0;

  private:
    Clock::time_point epoch;
    Clock::duration period;
    uint64_t slots, next = 0;
};

struct PacedResult
{
    uint64_t requested = 0, started = 0, completed = 0, missed = 0, errors = 0;
    int affinity_error = 0;
    LatencyHistogram latency, release_lateness, response;
    std::string json() const
    {
        return "{\"requested\":" + std::to_string(requested) +
               ",\"started\":" + std::to_string(started) +
               ",\"completed\":" + std::to_string(completed) +
               ",\"missed\":" + std::to_string(missed) + ",\"errors\":" + std::to_string(errors) +
               ",\"affinity_error\":" + std::to_string(affinity_error) +
               ",\"latency_us\":" + latency.json() +
               ",\"release_lateness_us\":" + release_lateness.json() +
               ",\"response_us\":" + response.json() + "}";
    }
    bool valid() const
    {
        return requested != 0 && errors == 0 && affinity_error == 0 &&
               double(completed) >= .98 * double(requested);
    }
};

template <class Operation>
void paced(PacedResult& result, Clock::time_point epoch, unsigned rate, unsigned duration,
           const std::atomic<bool>& cancelled, Operation operation)
{
    result.requested = uint64_t(rate) * duration;
    const auto period =
        std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / rate));
    const auto end = epoch + std::chrono::seconds(duration);
    Schedule schedule(epoch, period, result.requested);
    while (!cancelled && !schedule.done())
    {
        std::this_thread::sleep_until(schedule.release());
        auto now = Clock::now();
        schedule.skip_expired(now);
        if (cancelled || schedule.done())
            break;
        const auto release = schedule.release();
        // skip_expired can select a future release only at the end of the window.
        ++result.started;
        result.release_lateness.add(
            std::chrono::duration<double, std::micro>(now - release).count());
        if (!operation())
        {
            ++result.errors;
            break;
        }
        const auto finished = Clock::now();
        result.latency.add(std::chrono::duration<double, std::micro>(finished - now).count());
        result.response.add(std::chrono::duration<double, std::micro>(finished - release).count());
        if (finished <= end)
            ++result.completed;
        schedule.advance();
    }
    result.missed = result.requested - result.started;
}

constexpr unsigned comparison_seconds = 2, comparison_rpc_rate = 600;
constexpr unsigned comparison_memory_rate = 512, comparison_memory_bytes = 16384;
constexpr unsigned comparison_dma_rate = 256, comparison_dma_bytes = 4096;
constexpr std::array<std::string_view, 6> comparison_cases{
    "idle", "memory", "dma", "mixed", "upload_value", "upload_reference"};
inline std::string comparison_parameters(unsigned mask)
{
    std::string result = "{\"settle_seconds\":1.5,\"case_mask\":" + std::to_string(mask) +
                         ",\"window_seconds\":2,\"rpc_requests_per_second\":600,"
                         "\"memory_bytes_per_second\":16777216,\"memory_quantum_bytes\":16384,"
                         "\"memory_working_set_bytes\":8388608,\"dma_bytes_per_second\":"
                         "1048576,"
                         "\"dma_quantum_bytes\":4096,\"dma_payload_accounting\":\"one "
                         "direction, duplex verified\","
                         "\"release_policy\":\"absolute, skip expired slots, one outstanding "
                         "operation\","
                         "\"minimum_completed_fraction\":0.98,\"maximum_pair_load_difference\":"
                         "0.02,"
                         "\"observer_requests_allowed\":0,\"uart_load\":false";
    if (mask & 48)
        result += ",\"upload\":{\"payload_bytes\":" + std::to_string(upload_bytes) +
                  ",\"requests_per_second\":" + std::to_string(upload_requests_per_second) +
                  ",\"alternating_seeds\":[324508639,610839776],"
                  "\"validation\":\"every word checked before acknowledgement\"}";
    return result + '}';
}
} // namespace bist
#endif
