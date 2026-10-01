// examples/red-pitaya/tests/irq/main.cpp
// V1.3 (UIO): 10 ms AXI Timer + userspace IRQs via UIO,
//             primed on first IRQ, RT hints, and UIO opened by mem::timer_addr.

#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"

#include <cstdint>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <signal.h>
#include <dirent.h>
#include <cstdio>

namespace reg {
constexpr unsigned TCSR0 = 0x00; // Timer 0 Control/Status
constexpr unsigned TLR0  = 0x04; // Timer 0 Load
constexpr unsigned TCR0  = 0x08; // Timer 0 Counter
}

// TCSR0 bits (lean)
constexpr uint32_t TCSR_UDT0  = 1u << 1;  // 0=up, 1=down
constexpr uint32_t TCSR_ARHT0 = 1u << 4;  // auto-reload
constexpr uint32_t TCSR_LOAD0 = 1u << 5;  // load TLR0 -> TCR0 (w1)
constexpr uint32_t TCSR_ENIT0 = 1u << 6;  // IRQ enable
constexpr uint32_t TCSR_ENT0  = 1u << 7;  // timer enable
constexpr uint32_t TCSR_T0INT = 1u << 8;  // IRQ status (w1c)

// Graceful stop
static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

static inline double ms_between(const timespec& a, const timespec& b) {
    return (b.tv_sec - a.tv_sec) * 1000.0 + (b.tv_nsec - a.tv_nsec) / 1e6;
}

static void best_effort_rt() {
    // Lock memory and try to reduce jitter; ignore failures if unprivileged
    mlockall(MCL_CURRENT | MCL_FUTURE);
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set);
    sched_setaffinity(0, sizeof(set), &set);
    sched_param sp{}; sp.sched_priority = 80;
    sched_setscheduler(0, SCHED_FIFO, &sp);
}

// Open /dev/uioX whose map0/addr == want_base
static int open_uio_by_addr(uintptr_t want_base) {
    DIR* d = opendir("/sys/class/uio");
    if (!d) {
        log<PANIC>("opendir(/sys/class/uio): %s\n", std::strerror(errno));
        return -1;
    }
    int fd = -1;
    for (dirent* e; (e = readdir(d)); ) {
        if (std::strncmp(e->d_name, "uio", 3) != 0) continue;
        char path[256];
        std::snprintf(path, sizeof(path), "/sys/class/uio/%s/maps/map0/addr", e->d_name);
        FILE* f = std::fopen(path, "r");
        if (!f) continue;
        unsigned long long addr = 0;
        const int n = std::fscanf(f, "%llx", &addr);
        std::fclose(f);
        if (n == 1 && addr == static_cast<unsigned long long>(want_base)) {
            char dev[64];
            std::snprintf(dev, sizeof(dev), "/dev/%s", e->d_name);
            fd = ::open(dev, O_RDWR | O_CLOEXEC);
            if (fd < 0) {
                log<PANIC>("open(%s): %s\n", dev, std::strerror(errno));
            } else {
                log<INFO>("Using %s for IRQs (addr=0x%llx)\n",
                                     dev, static_cast<unsigned long long>(want_base));
            }
            break;
        }
    }
    closedir(d);
    if (fd < 0) {
        log<PANIC>("No UIO device with base 0x%llx\n",
                              static_cast<unsigned long long>(want_base));
    }
    return fd;
}

int main() {
    // Signals
    signal(SIGINT,  on_sigint);
    signal(SIGTERM, on_sigint);

    hw::FpgaManager fpga;
    hw::ZynqFclk fclk;
    hw::MemoryManager mm;

    if (fpga.load_bitstream() < 0) {
        log<PANIC>("E1: Failed to load bitstream\n");
        return -1;
    }
    if (mm.open() < 0) {
        log<PANIC>("E2: Failed to open /dev/mem\n");
        return -1;
    }

    // FCLK0 = 100 MHz (drives AXI Timer clock)
    fclk.set("fclk0", 100000000);
    rt::systemd::notify_ready();

    // Light RT tuning
    best_effort_rt();

    auto& timer = mm.get<mem::timer>();

    // --- Program 10 ms @ 100 MHz (1,000,000 cycles) ---
    const uint32_t period_cycles = 1'000'000 - 1;
    const uint32_t cfg = TCSR_UDT0 | TCSR_ARHT0 | TCSR_ENIT0;

    timer.write<reg::TCSR0>(TCSR_T0INT);                 // clear any pending
    timer.write<reg::TLR0>(period_cycles);               // load value
    timer.write<reg::TCSR0>(TCSR_T0INT | TCSR_LOAD0);    // transfer TLR0->TCR0
    timer.write<reg::TCSR0>(TCSR_T0INT | cfg);           // config (down, autoreload, IRQ)
    timer.write<reg::TCSR0>(TCSR_T0INT | cfg | TCSR_ENT0); // start

    // --- Userspace IRQ via UIO (open by mem::timer_addr) ---
    int uio = open_uio_by_addr(mem::timer_addr);
    if (uio < 0) return -1;

    uint32_t arm = 1;
    if (::write(uio, &arm, sizeof(arm)) != sizeof(arm)) {
        log<PANIC>("E5: uio enable: %s\n", std::strerror(errno));
        ::close(uio);
        return -1;
    }

    // Prime on first IRQ: sync point (no period printed here)
    {
        struct pollfd pfd{uio, POLLIN, 0};
        int pr = ::poll(&pfd, 1, 2000);
        if (pr <= 0) {
            log<PANIC>("E6: poll during prime %s\n", pr==0 ? "timeout" : std::strerror(errno));
            ::close(uio);
            return -1;
        }
        uint32_t irqcnt = 0;
        if (::read(uio, &irqcnt, sizeof(irqcnt)) != sizeof(irqcnt)) {
            log<PANIC>("E7: uio read during prime: %s\n", std::strerror(errno));
            ::close(uio);
            return -1;
        }
        timer.write<reg::TCSR0>(TCSR_T0INT | cfg | TCSR_ENT0); // ACK and keep running
        if (::write(uio, &arm, sizeof(arm)) != sizeof(arm)) {
            log<PANIC>("E8: uio re-arm during prime: %s\n", std::strerror(errno));
            ::close(uio);
            return -1;
        }
        log<INFO>("Synced on first IRQ (cnt=%u)\n", irqcnt);
    }

    // Measure and print IRQ-to-IRQ period
    timespec t_prev{}, t_now{};
    clock_gettime(CLOCK_MONOTONIC_RAW, &t_prev);

    for (int i = 1; !g_stop && i <= 10; ++i) {
        struct pollfd pfd{uio, POLLIN, 0};
        int pr = ::poll(&pfd, 1, 2000); // 2 s timeout
        if (pr <= 0) {
            log<PANIC>("E6: poll %s\n", pr==0 ? "timeout" : std::strerror(errno));
            break;
        }

        uint32_t irqcnt = 0;
        if (::read(uio, &irqcnt, sizeof(irqcnt)) != sizeof(irqcnt)) {
            log<PANIC>("E7: uio read: %s\n", std::strerror(errno));
            break;
        }

        clock_gettime(CLOCK_MONOTONIC_RAW, &t_now);
        double period_ms = ms_between(t_prev, t_now);
        t_prev = t_now;

        // ACK device IRQ and keep timer running (preserve ENT0|cfg)
        timer.write<reg::TCSR0>(TCSR_T0INT | cfg | TCSR_ENT0);

        // (Optional) observe counter value
        uint32_t tcr = timer.read<reg::TCR0>();
        log<INFO>("[IRQ %u] period=%.3f ms  TCR0=%u\n", irqcnt, period_ms, tcr);

        // Re-arm UIO delivery
        if (::write(uio, &arm, sizeof(arm)) != sizeof(arm)) {
            log<PANIC>("E8: uio re-enable: %s\n", std::strerror(errno));
            break;
        }
    }

    ::close(uio);

    // Stop timer (clear ENT0, keep config)
    timer.write<reg::TCSR0>(TCSR_T0INT | cfg);

    return 0;
}