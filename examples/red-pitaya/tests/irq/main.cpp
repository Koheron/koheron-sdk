// examples/red-pitaya/tests/irq/main.cpp
// Load bitstream, set FCLK0=100 MHz, program AXI Timer for 10 ms IRQs,
// and sanity-check the counter is ticking.

#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/context/memory_manager.hpp"
#include "server/context/fpga_manager.hpp"
#include "server/context/zynq_fclk.hpp"

#include <cstdint>
#include <unistd.h> // usleep

// -----------------------------------------------------------------------------
// AXI Timer register offsets (non-type template constants)
// -----------------------------------------------------------------------------
namespace reg {
constexpr unsigned TCSR0 = 0x00; // Timer 0 Control/Status
constexpr unsigned TLR0  = 0x04; // Timer 0 Load
constexpr unsigned TCR0  = 0x08; // Timer 0 Counter
// (Timer1 not used here)
}

// -----------------------------------------------------------------------------
// Bits for TCSR0 (lean: only what we use)
// -----------------------------------------------------------------------------
constexpr uint32_t TCSR_UDT0  = 1u << 1;  // 0=up, 1=down
constexpr uint32_t TCSR_ARHT0 = 1u << 4;  // auto-reload
constexpr uint32_t TCSR_LOAD0 = 1u << 5;  // load TLR0 -> TCR0 (write 1)
constexpr uint32_t TCSR_ENIT0 = 1u << 6;  // interrupt enable
constexpr uint32_t TCSR_ENT0  = 1u << 7;  // timer enable
constexpr uint32_t TCSR_T0INT = 1u << 8;  // IRQ status (write 1 to clear)

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

    // FCLK0 = 100 MHz (drives AXI Timer clock)
    fclk.set("fclk0", 100000000, true);
    systemd::notify_ready();

    auto& timer = mm.get<mem::timer>();

    // 10 ms @ 100 MHz = 1,000,000 cycles; program TLR0 with (period - 1).
    const uint32_t period_cycles = 1'000'000 - 1;

    // Clear any pending IRQ
    timer.write<reg::TCSR0>(TCSR_T0INT);

    // Program period and transfer to counter
    timer.write<reg::TLR0>(period_cycles);
    timer.write<reg::TCSR0>(TCSR_T0INT | TCSR_LOAD0); // load TLR0 -> TCR0

    // Configure: down-count, auto-reload, interrupt enable (keep ENT0=0 for now)
    const uint32_t cfg = TCSR_UDT0 | TCSR_ARHT0 | TCSR_ENIT0;
    timer.write<reg::TCSR0>(TCSR_T0INT | cfg);

    // Start timer
    timer.write<reg::TCSR0>(TCSR_T0INT | cfg | TCSR_ENT0);

    const uint32_t c0 = timer.read<reg::TCR0>();
    usleep(5000); // 5 ms
    const uint32_t c1 = timer.read<reg::TCR0>();

    // Print c1-c0 (handle down-count + auto-reload wrap)
    const uint32_t period_plus_one = period_cycles + 1; // 1,000,000
    const uint32_t ticks = (c0 >= c1) ? (c0 - c1) : (c0 + period_plus_one - c1);
    const uint32_t us = ticks / 100; // 100 MHz => 100 cycles per microsecond

    koheron::print<INFO>("AXI timer delta: %u ticks (~%u us), TCR0 %u -> %u\n",
                        ticks, us, c0, c1);
    return 0;
}
