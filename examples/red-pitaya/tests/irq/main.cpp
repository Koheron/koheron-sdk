// examples/red-pitaya/tests/irq/main.cpp
// V1.3 (UIO): 10 ms AXI Timer + userspace IRQs via UIO,
//             primed on first IRQ, RT hints, and UIO opened by mem::timer_addr.

#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"
#include "server/drivers/uio.hpp"

#include <chrono>
#include <format>

#include <time.h>
#include <sched.h>
#include <sys/mman.h>

using namespace std::chrono_literals;
using clk = std::chrono::steady_clock;

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

static void best_effort_rt() {
    // Lock memory and try to reduce jitter; ignore failures if unprivileged
    mlockall(MCL_CURRENT | MCL_FUTURE);
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set);
    sched_setaffinity(0, sizeof(set), &set);
    sched_param sp{}; sp.sched_priority = 80;
    sched_setscheduler(0, SCHED_FIFO, &sp);
}

int main() {
    hw::FpgaManager fpga;
    hw::ZynqFclk fclk;
    hw::MemoryManager mm;

    if (fpga.load_bitstream() < 0) {
        rt::print<PANIC>("E1: Failed to load bitstream\n");
        return -1;
    }
    if (mm.open() < 0) {
        rt::print<PANIC>("E2: Failed to open /dev/mem\n");
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

    // --- Userspace IRQ via UIO (open by mem::timer) ---
    Uio<mem::timer> timer_uio;

    if (timer_uio.open() < 0) {
        return -1;
    }

    // -----------------------------------------
    // Synchronous API
    // -----------------------------------------

    if (!timer_uio.arm_irq()) {
        return -1;
    }

    // Prime on first IRQ: sync point (no period printed here)
    int irqcnt = timer_uio.wait_for_irq(2s);

    if (irqcnt < 0) {
        return -1;
    }

    timer.write<reg::TCSR0>(TCSR_T0INT | cfg | TCSR_ENT0); // ACK and keep running

    if (!timer_uio.arm_irq()) {
        return -1;
    }

    rt::print_fmt<INFO>("Synced on first IRQ (cnt={})\n", irqcnt);

    // Measure and print IRQ-to-IRQ period
    auto t_prev = clk::now();

    for (int i = 1; i <= 10; ++i) {
        irqcnt = timer_uio.wait_for_irq(2s);

        if (irqcnt < 0) {
            return -1;
        }

        const auto t_now = clk::now();
        const auto period = t_now - t_prev;
        t_prev = t_now;

        // ACK device IRQ and keep timer running (preserve ENT0|cfg)
        timer.write<reg::TCSR0>(TCSR_T0INT | cfg | TCSR_ENT0);
        uint32_t tcr = timer.read<reg::TCR0>();

        const auto ms = std::chrono::duration<double, std::milli>(period);
        rt::print_fmt<INFO>("[IRQ {}] period={:%Q %q} TCR0={}\n",
                                irqcnt, ms, tcr);

        if (!timer_uio.arm_irq()) {
            return -1;
        }
    }

    // -----------------------------------------
    // Asynchronous API
    // -----------------------------------------

    std::atomic<bool> done = false;
    int cnt = 0;
    t_prev = clk::now();

    timer_uio.listen(
        [&] (int irqcnt) {
            if (irqcnt <= 0) { // error/timeout policy: stop
                done = true;
                return;
            }

            const auto t_now = clk::now();
            const auto period = t_now - t_prev;
            t_prev = t_now;

            // ACK device IRQ and keep timer running
            timer.write<reg::TCSR0>(TCSR_T0INT | cfg | TCSR_ENT0);
            const auto ms = std::chrono::duration<double, std::milli>(period);
            uint32_t tcr = timer.read<reg::TCR0>();
            rt::print_fmt<INFO>("ASYNC: [IRQ {}] period={:%Q %q} TCR0={}\n",
                                    irqcnt, ms, tcr);

            cnt += irqcnt;
            if (cnt > 10) {
                done = true;
            }
        },
        2s  // timeout passed to wait_for_irq inside the worker
    );

    // Keep main alive until done
    while (!done) {
        std::this_thread::sleep_for(10ms);
    }

    timer_uio.unlisten();
    // Stop timer (clear ENT0, keep config)
    timer.write<reg::TCSR0>(TCSR_T0INT | cfg);
    return 0;
}
