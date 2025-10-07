#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/context/memory_manager.hpp"
#include "server/context/fpga_manager.hpp"
#include "server/context/zynq_fclk.hpp"

#include <chrono>
#include <thread>
#include <cstdint>

int main() {
    FpgaManager fpga;
    ZynqFclk fclk;
    MemoryManager mm;

    if (fpga.load_bitstream() < 0) {
        koheron::print<PANIC>("Failed to load bitstream.\n");
        return -1;
    }
    if (mm.open() < 0) {
        koheron::print<PANIC>("Failed to open memory");
        return -1;
    }

    // Set fabric clock; change to 200'000'000 to test 200 MHz
    fclk.set("fclk0", 100000000, true);
    systemd::notify_ready();

    auto& gpio = mm.get<mem::gpio>();

    using namespace std::chrono;

    // Warm-up read
    (void)gpio.read<0x0, uint32_t>();

    // Measure over ~10 ms
    const auto t0 = steady_clock::now();
    const uint32_t v0 = gpio.read<0x0, uint32_t>();

    std::this_thread::sleep_for(milliseconds(10));

    const auto t1 = steady_clock::now();
    const uint32_t v1 = gpio.read<0x0, uint32_t>();

    // 32-bit wrap-around
    const uint32_t delta = (v1 >= v0) ? (v1 - v0) : (0xFFFFFFFFu - v0 + 1u + v1);
    const double   dt_s  = duration_cast<duration<double>>(t1 - t0).count();
    const double   f_hz  = delta / dt_s;

    koheron::print<INFO>("GPIO counter delta=%u over %.6f s -> FCLK ≈ %.3f MHz\n",
                         delta, dt_s, f_hz / 1e6);


    fclk.set("fclk0", 187500000, true);

    // .......

    fclk.set("fclk0", 200000000, true);

    // .......

    return 0;
}
