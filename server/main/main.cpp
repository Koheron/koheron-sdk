/// Main file for koheron-server
///
/// (c) Koheron

#include "server/hardware/memory_manager.hpp"
#include "server/hardware/spi_manager.hpp"
#include "server/hardware/i2c_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"
#include "server/hardware/fpga_manager.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/runtime/config_manager.hpp"
#include "server/runtime/executor.hpp"

#include "server/network/listener_manager.hpp"
#include "server/executor/executor.hpp"
#include <drivers.hpp> // For call to Common

#include <atomic>
#include <cstdlib>

using services::provide;

int main() {
    // /!\ Services initialization order matters

    // ---------- Hardware services ----------

    auto fpga = provide<hw::FpgaManager>();

    if (fpga->load_bitstream() < 0) {
        log<PANIC>("Failed to load bitstream. Exiting server...\n");
        std::exit(EXIT_FAILURE);
    }

    // Set Zynq clocks before starting drivers
    auto fclk = provide<hw::ZynqFclk>();
    zynq_clocks::set_clocks(*fclk);

    if (provide<hw::MemoryManager>()->open() < 0 ||
        provide<hw::SpiManager>()->init()    < 0 ||
        provide<hw::I2cManager>()->init()    < 0) {
            log<PANIC>("Hardware services initialization failed\n");
            std::exit(EXIT_FAILURE);
    }

    // ---------- Runtime services ----------

    if (provide<rt::ConfigManager>()->init() < 0) {
        std::exit(EXIT_FAILURE);
    }

    // On driver allocation failure
    auto on_fail = [&]([[maybe_unused]] rt::driver_id id,
                       [[maybe_unused]] std::string_view name) {
        std::exit(EXIT_FAILURE);
    };

    auto dm = provide<rt::DriverManager>(on_fail);

    // If there is a Common driver with an init() method we call it
    if constexpr (drivers::table::has_driver<Common>) {
        if constexpr (rt::HasInit<Common>) {
            dm->get<Common>().init();
        }
    }

    // ---------- Server ----------

    rt::provide_executor<koheron::Executor>();
    return net::run_server("Koheron server ready\n");
}
