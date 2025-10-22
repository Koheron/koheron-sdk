// examples/alpha250/fft/app
//
// Example of overriding the default server entry point (server/main/main.cpp)
// with a custom entry point.
//
// This example bootstraps everything needed to run ALPHA250 peripherals and
// starts a server with a custom Executor

#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/spi_manager.hpp"
#include "server/hardware/i2c_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/runtime/executor.hpp"

#include "server/network/listener_manager.hpp"
#include "server/network/session_manager.hpp"
#include "server/network/commands.hpp"

#include "boards/alpha250/drivers/common.hpp"
#include "examples/alpha250/fft/fft.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <string>

using services::provide;
using services::require;

class AppExecutor final : public rt::IExecutor {
  public:
    int handle_app(net::Command& cmd) override {
        if (cmd.driver != 0xFFFF) {
            return 0;
        }

        if (cmd.operation == 0) {
            auto& sess = require<net::SessionManager>().get_session(cmd.session_id);
            sess.log_infos();
            sess.log_rates();

            std::string s;
            cmd.read_one(s);
            logf("echo: {}", s);
            return cmd.send(s);
        }

        return 0;
    }
};

int main() {
    std::atomic<bool> exit_all = false;

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
        exit_all = true;
    };
    auto dm = provide<rt::DriverManager>(on_fail);

    // Initialize ALPHA250 peripherals. Common::init() performs board-level setup.
    dm->get<Common>().init();

    // ----------- Run server

    rt::provide_executor<AppExecutor>();
    return net::run_server("FFT app is ready\n");
}
