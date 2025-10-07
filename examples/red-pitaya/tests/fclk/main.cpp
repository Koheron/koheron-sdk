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

    auto measure_mhz = [&]() -> double {
        // Warm-up read
        (void)gpio.read<0x0, uint32_t>();

        const auto t0 = steady_clock::now();
        const uint32_t v0 = gpio.read<0x0, uint32_t>();

        std::this_thread::sleep_for(milliseconds(10));

        const auto t1 = steady_clock::now();
        const uint32_t v1 = gpio.read<0x0, uint32_t>();

        const uint32_t delta = (v1 >= v0) ? (v1 - v0) : (0xFFFFFFFFu - v0 + 1u + v1);
        const double dt_s = duration_cast<duration<double>>(t1 - t0).count();
        return (delta / dt_s) / 1e6; // MHz
    };

    // --- Measure @ 100 MHz --------------------------------------------------------
    {
        const double f_mhz = measure_mhz();
        koheron::print<INFO>("FCLK0 set=100.000 MHz -> measured ≈ %.3f MHz\n", f_mhz);
    }

    // Switch to 125 MHz, allow a brief settle, then measure
    fclk.set("fclk0", 125000000, true);
    std::this_thread::sleep_for(milliseconds(20));
    {
        const double f_mhz = measure_mhz();
        koheron::print<INFO>("FCLK0 set=125.000 MHz -> measured ≈ %.3f MHz\n", f_mhz);
    }

    // Switch to 187.5 MHz, allow a brief settle, then measure
    fclk.set("fclk0", 187500000, true);
    std::this_thread::sleep_for(milliseconds(20));
    {
        const double f_mhz = measure_mhz();
        koheron::print<INFO>("FCLK0 set=187.500 MHz -> measured ≈ %.3f MHz\n", f_mhz);
    }

    // Switch to 250 MHz, allow a brief settle, then measure
    fclk.set("fclk0", 250000000, true);
    std::this_thread::sleep_for(milliseconds(20));
    {
        const double f_mhz = measure_mhz();
        koheron::print<INFO>("FCLK0 set=250.000 MHz -> measured ≈ %.3f MHz\n", f_mhz);
    }

    return 0;
}
