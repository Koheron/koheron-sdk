// examples/alpha250/fft/app
//
// Example of overriding the default server entry point (server/main/main.cpp)
// with a custom entry point.
//
// This example bootstraps everything needed to run ALPHA250 peripherals and
// logs ADC raw data.

#include "server/runtime/syslog.hpp"
#include "server/hardware/zynq_fclk.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/systemd.hpp"
#include "server/runtime/driver_manager.hpp"

#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/spi_manager.hpp"
#include "server/hardware/i2c_manager.hpp"

#include "boards/alpha250/drivers/common.hpp"
#include "examples/alpha250/fft/fft.hpp"

#include <chrono>
#include <thread>
#include <cstdlib>

using services::provide;

int main() {
    // ------------------- Initialization -------------------
    // NOTE: Initialization order matters.
    // Initialize low-level hardware services (memory, FPGA, clocks, buses) before
    // higher-level runtime services (drivers, systemd notify, signals).

    // 1) Load the FPGA bitstream.
    auto fpga = provide<hw::FpgaManager>();
    if (fpga->load_bitstream() < 0) {
        log<PANIC>("Failed to load bitstream. Exiting...\n");
        std::exit(EXIT_FAILURE);
    }

    // 2) Configure Zynq PL clocks.
    auto fclk = provide<hw::ZynqFclk>();
    zynq_clocks::set_clocks(*fclk);

    // 3) Bring up basic hardware services needed by drivers.
    if (provide<hw::MemoryManager>()->open() < 0 ||
        provide<hw::SpiManager>()->init()    < 0 ||
        provide<hw::I2cManager>()->init()    < 0) {
        log<PANIC>("Hardware services initialization failed\n");
        std::exit(EXIT_FAILURE);
    }

    // 4) Start the driver manager. It will construct/host the drivers declared
    //    in drivers_config.hpp. If any driver fails to initialize, exit.
    auto on_fail = [&]([[maybe_unused]] rt::driver_id id,
                       [[maybe_unused]] std::string_view name) {
        std::exit(EXIT_FAILURE);
    };
    auto dm = provide<rt::DriverManager>(on_fail);

    // Initialize ALPHA250 peripherals. Common::init() performs board-level setup.
    dm->get<Common>().init();

    // Signal systemd that the application is ready to serve.
    rt::systemd::notify_ready("FFT app is ready");

    // ---------------- End initialization ------------------

    // ------------------- Application logic ----------------

    auto& fft = dm->get<FFT>();
    logf("FFT size = {} points\n", fft.get_fft_size());

    while (true) {
        using namespace std::chrono_literals;
        auto data = fft.get_adc_raw_data(10);
        logf("FFT data = {}, {}\n", data[0], data[1]);
        std::this_thread::sleep_for(10ms);
    }

    return 0;
}
