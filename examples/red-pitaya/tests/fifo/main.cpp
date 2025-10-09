#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/context/memory_manager.hpp"
#include "server/context/fpga_manager.hpp"
#include "server/context/zynq_fclk.hpp"

#include <cstdint>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <sys/mman.h>

// Minimal AXI4-Stream FIFO regs (PG080)
namespace reg { namespace fifo {
constexpr uint32_t ISR   = 0x00; // Interrupt Status (RW1C)
constexpr uint32_t IER   = 0x04; // Interrupt Enable (R/W)
constexpr uint32_t TDFR  = 0x08; // TX FIFO Reset (W: 0xA5)
constexpr uint32_t RDFR  = 0x18; // RX FIFO Reset (W: 0xA5)
constexpr uint32_t RDFO  = 0x1C; // RX FIFO Occupancy (R, words)
constexpr uint32_t RDFD  = 0x20; // RX Data (R, 32-bit)
constexpr uint32_t SRR   = 0x28; // AXI4-Stream Reset (W: 0xA5)
}}

// --------- RT helpers ----------
static void set_realtime() {
    // Lock current and future pages
    mlockall(MCL_CURRENT | MCL_FUTURE);

    // Pin to CPU0 (reduce migrations)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    // SCHED_FIFO with mid-high priority
    sched_param sp{};
    sp.sched_priority = 80;
    sched_setscheduler(0, SCHED_FIFO, &sp);
}

int main() {
    FpgaManager fpga;
    ZynqFclk fclk;
    MemoryManager mm;

    if (fpga.load_bitstream() < 0) {
        koheron::print<PANIC>("Failed to load bitstream.\n");
        return -1;
    }
    if (mm.open() < 0) {
        koheron::print<PANIC>("Failed to open memory\n");
        return -1;
    }

    fclk.set("fclk0", 100000000); // AXI
    fclk.set("fclk1", 2500000);   // stream source
    systemd::notify_ready();

    set_realtime();

    auto& fifo = mm.get<mem::fifo>();

    // Reset control bits and keep interrupts off (we poll)
    fifo.write<reg::fifo::SRR>(0xA5);
    fifo.write<reg::fifo::RDFR>(0xA5);
    fifo.write<reg::fifo::TDFR>(0xA5);
    fifo.write<reg::fifo::ISR>(0xFFFFFFFF);
    fifo.write<reg::fifo::IER>(0x00000000);

    koheron::print<INFO>("FIFO probe: ISR=0x%08x IER=0x%08x RDFO=%u\n",
                         fifo.read<reg::fifo::ISR>(),
                         fifo.read<reg::fifo::IER>(),
                         fifo.read<reg::fifo::RDFO>());

    // ---- PRE-DRAIN (get ahead of the stream before measuring) ----
    // Drain aggressively for ~50 ms so we start with a near-empty FIFO.
    {
        using clock = std::chrono::steady_clock;
        const auto pre_end = clock::now() + std::chrono::milliseconds(50);
        while (clock::now() < pre_end) {
            uint32_t occ = fifo.read<reg::fifo::RDFO>();
            while (occ--) { (void)fifo.read<reg::fifo::RDFD>(); }
        }
    }

    // Establish baseline
    uint32_t last = 0;
    bool have_last = false;
    for (;;) {
        uint32_t occ = fifo.read<reg::fifo::RDFO>();
        if (occ) { last = fifo.read<reg::fifo::RDFD>(); have_last = true; break; }
    }
    fifo.write<reg::fifo::ISR>(0xFFFFFFFF); // clear any latched status

    // Counters
    uint64_t words = 0;
    uint64_t lost  = 0;
    uint64_t dupes = 0;

    // Timers
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    auto next_stat   = start + std::chrono::seconds(1);
    const auto stop  = start + std::chrono::seconds(10);

    // ---- 10 s tight loop ----
    while (clock::now() < stop) {
        // Drain whatever is there; no artificial cap
        uint32_t occ = fifo.read<reg::fifo::RDFO>();
        while (occ--) {
            uint32_t w = fifo.read<reg::fifo::RDFD>();
            if (have_last) {
                uint32_t d = w - last; // modulo 2^32
                if (d == 0) ++dupes;
                else if (d != 1) lost += static_cast<uint64_t>(d - 1);
            }
            last = w;
            have_last = true;
            ++words;
        }

        // Stats & occasional ISR check (1 Hz to reduce AXI-Lite traffic)
        auto now = clock::now();
        if (now >= next_stat) {
            const uint32_t st = fifo.read<reg::fifo::ISR>();
            if (st) {
                fifo.write<reg::fifo::ISR>(st);
                // Log only on error-like conditions (optional)
                if (st & 0xE0000000u) {
                    koheron::print<ERROR>("FIFO ISR=0x%08x\n", st);
                }
            }
            koheron::print<INFO>(
                "RX: words=%llu lost=%llu dupes=%llu last=0x%08x (occ=%u)\n",
                static_cast<unsigned long long>(words),
                static_cast<unsigned long long>(lost),
                static_cast<unsigned long long>(dupes),
                last, fifo.read<reg::fifo::RDFO>()
            );
            next_stat += std::chrono::seconds(1);
        }
    }

    koheron::print<INFO>(
        "DONE (10s): words=%llu lost=%llu dupes=%llu last=0x%08x\n",
        static_cast<unsigned long long>(words),
        static_cast<unsigned long long>(lost),
        static_cast<unsigned long long>(dupes),
        last
    );
    return 0;
}
