#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"

#include <cstdint>
#include <chrono>

// AXI FIFO MM-S byte offsets (PG080)
namespace reg { namespace fifo {
constexpr uint32_t ISR  = 0x00; // Interrupt Status (RW1C)
constexpr uint32_t IER  = 0x04; // Interrupt Enable
constexpr uint32_t TDFR = 0x08; // TX FIFO Reset (W: 0xA5)
constexpr uint32_t RDFR = 0x18; // RX FIFO Reset (W: 0xA5)
constexpr uint32_t RDFO = 0x1C; // RX FIFO Occupancy (words)
constexpr uint32_t RDFD = 0x20; // RX Data (32-bit)
constexpr uint32_t SRR  = 0x28; // Stream Reset (W: 0xA5)
}}

// Batch size (words)
static constexpr uint32_t CHUNK = 1024;

int main() {
    hw::FpgaManager fpga;
    hw::ZynqFclk fclk;
    hw::MemoryManager mm;

    if (fpga.load_bitstream() < 0) {
        log<PANIC>("Failed to load bitstream.\n");
        return -1;
    }
    if (mm.open() < 0) {
        log<PANIC>("Failed to open memory\n");
        return -1;
    }

    fclk.set("fclk0", 100000000);   // AXI/control
    fclk.set("fclk1", 10000000);     // stream source (e.g., 2.5e6 or 5e6)
    rt::systemd::notify_ready();

    auto& fifo = mm.get<mem::fifo>();

    // One-time bring-up
    fifo.write<reg::fifo::SRR>(0xA5);
    fifo.write<reg::fifo::RDFR>(0xA5);
    fifo.write<reg::fifo::TDFR>(0xA5);
    fifo.write<reg::fifo::ISR>(0xFFFFFFFF);
    fifo.write<reg::fifo::IER>(0x00000000);

    log<INFO>("FIFO probe: ISR=0x%08x IER=0x%08x RDFO=%u\n",
                         fifo.read<reg::fifo::ISR>(),
                         fifo.read<reg::fifo::IER>(),
                         fifo.read<reg::fifo::RDFO>());

    using clock = std::chrono::steady_clock;

    // Pre-drain briefly (no reset) so we start caught up
    const auto pre_until = clock::now() + std::chrono::milliseconds(30);
    while (clock::now() < pre_until) {
        uint32_t occ = fifo.read<reg::fifo::RDFO>();
        while (occ--) { (void)fifo.read<reg::fifo::RDFD>(); }
    }

    // Baseline at T0: wait for at least 1 word, take it, clear ISR
    while (fifo.read<reg::fifo::RDFO>() == 0) {}
    uint32_t last = fifo.read<reg::fifo::RDFD>();
    fifo.write<reg::fifo::ISR>(0xFFFFFFFF);

    // Counters
    uint64_t words = 0;   // received words (counted in CHUNKs)
    uint64_t lost  = 0;   // missing increments

    // 10 s window with 1 Hz stats
    const auto start = clock::now();
    auto next_stat   = start + std::chrono::seconds(1);
    const auto stop  = start + std::chrono::seconds(10);

    while (clock::now() < stop) {
        // Wait until occupancy reaches the batch threshold
        uint32_t occ = fifo.read<reg::fifo::RDFO>();
        if (occ < CHUNK) {
            // Busy-wait for minimum latency; add a tiny pause if you want to save CPU
            continue;
        }

        // --- Read exactly CHUNK words, remember only the last one ---
        uint32_t last_in_batch = last;
        for (uint32_t i = 0; i < CHUNK; i++) {
            last_in_batch = fifo.read<reg::fifo::RDFD>();
        }

        // Single continuity check for the batch:
        // expected increment is CHUNK; missing = max(0, delta - CHUNK)
        const uint32_t delta = last_in_batch - last; // modulo 2^32
        if (delta > CHUNK) {
            lost += static_cast<uint64_t>(delta - CHUNK);
        }

        last  = last_in_batch;
        words += CHUNK;

        // 1 Hz stats (loss %)
        const auto now = clock::now();
        if (now >= next_stat) {
            const uint64_t expected = words + lost;
            const double loss_pct = expected ? (100.0 * double(lost) / double(expected)) : 0.0;
            log<INFO>(
                "RX: total=%llu lost=%llu loss=%.3f%% last=0x%08x (occ=%u)\n",
                static_cast<unsigned long long>(expected),
                static_cast<unsigned long long>(lost),
                loss_pct,
                last,
                fifo.read<reg::fifo::RDFO>()
            );
            next_stat += std::chrono::seconds(1);
        }
    }

    // Final summary
    const uint64_t expected = words + lost;
    const double loss_pct = expected ? (100.0 * double(lost) / double(expected)) : 0.0;
    log<INFO>("DONE (10s): total=%llu lost=%llu loss=%.3f%% last=0x%08x\n",
                         static_cast<unsigned long long>(expected),
                         static_cast<unsigned long long>(lost),
                         loss_pct,
                         last);
    return 0;
}
