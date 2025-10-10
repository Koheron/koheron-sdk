#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/runtime/services.hpp"
#include "server/context/memory_manager.hpp"
#include "server/context/fpga_manager.hpp"
#include "server/context/zynq_fclk.hpp"
#include "server/drivers/fifo.hpp"

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
    FpgaManager fpga;

    if (fpga.load_bitstream() < 0) {
        koheron::print<PANIC>("Failed to load bitstream.\n");
        return -1;
    }

    auto mm = services::provide<MemoryManager>();

    if (mm->open() < 0) {
        koheron::print<PANIC>("Failed to open memory\n");
        return -1;
    }

    ZynqFclk fclk;
    fclk.set("fclk0", 100000000); // AXI/control
    fclk.set("fclk1", 10000000);  // stream source (e.g., 2.5e6 or 5e6)
    systemd::notify_ready();

    // auto& fifo = mm->get<mem::fifo>();
    Fifo<mem::fifo> fifo; // Requires a MemoryManager service
    fifo.reset();
    fifo.probe();

    using clock = std::chrono::steady_clock;

    // Pre-drain briefly (no reset) so we start caught up
    const auto pre_until = clock::now() + std::chrono::milliseconds(30);
    while (clock::now() < pre_until) {
        uint32_t occ = fifo.occupancy();
        while (occ--) { (void)fifo.read(); }
    }

    // Baseline at T0: wait for at least 1 word, take it, clear ISR
    while (fifo.occupancy() == 0) {}
    uint32_t last = fifo.read();
    // fifo.write<reg::fifo::ISR>(0xFFFFFFFF);

    // Counters
    uint64_t words = 0;   // received words (counted in CHUNKs)
    uint64_t lost  = 0;   // missing increments

    // 10 s window with 1 Hz stats
    const auto start = clock::now();
    auto next_stat   = start + std::chrono::seconds(1);
    const auto stop  = start + std::chrono::seconds(10);

    // Estimated word rate for wait timeout computation
    // (10 MHz stream -> ~10M words/s if one word per tick)
    const float fs_words_per_sec = 10'000'000.f;

    while (clock::now() < stop) {
        // Wait until occupancy reaches the batch threshold
        if (fifo.occupancy() < CHUNK) {
            fifo.wait_for_data(CHUNK, fs_words_per_sec);  // blocks on UIO IRQ
            if (fifo.occupancy() < CHUNK) continue;      // re-check; harmless if spurious wake
        }

        // --- Read exactly CHUNK words, remember only the last one ---
        uint32_t last_in_batch = last;
        for (uint32_t i = 0; i < CHUNK; i++) {
            last_in_batch = fifo.read();
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
            koheron::print_fmt<INFO>(
                "RX: total={} lost={} loss={:.3f}% last={:#010x} (occ={})\n",
                expected, lost, loss_pct, last, fifo.occupancy());
            next_stat += std::chrono::seconds(1);
        }
    }

    // Final summary
    const uint64_t expected = words + lost;
    const double loss_pct = expected ? (100.0 * double(lost) / double(expected)) : 0.0;
    koheron::print_fmt<INFO>("DONE (10s): total={} lost={} loss={:.3f}% last={:#010x}\n",
                         expected, lost, loss_pct, last);
    return 0;
}
