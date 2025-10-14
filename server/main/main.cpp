/// Main file for koheron-server
///
/// (c) Koheron

#include "server/hardware/memory_manager.hpp"
#include "server/hardware/spi_manager.hpp"
#include "server/hardware/i2c_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"
#include "server/hardware/fpga_manager.hpp"

#include "server/runtime/signal_handler.hpp"
#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/drivers_manager.hpp"
#include "server/runtime/config_manager.hpp"

#include "server/core/server.hpp"
#include "server/core/session_manager.hpp"
#include "server/core/drivers_executor.hpp"

int main() {
    using namespace koheron;

    // /!\ Services initialization order matters

    // ---------- Hardware services ----------

    auto fpga = services::provide<hw::FpgaManager>();

    if (fpga->load_bitstream() < 0) {
        log<PANIC>("Failed to load bitstream. Exiting server...\n");
        std::exit(EXIT_FAILURE);
    }

    // Set Zynq clocks before starting drivers
    auto fclk = services::provide<hw::ZynqFclk>();
    zynq_clocks::set_clocks(*fclk);

    if (services::provide<hw::MemoryManager>()->open() < 0 ||
        services::provide<hw::SpiManager>()->init()    < 0 ||
        services::provide<hw::I2cManager>()->init()    < 0) {
            log<PANIC>("Hardware services initialization failed\n");
            std::exit(EXIT_FAILURE);
    }

    // ---------- Runtime services ----------

    if (services::provide<rt::ConfigManager>()->init() < 0) {
        std::exit(EXIT_FAILURE);
    }

    // On driver allocation failure
    auto on_fail = []([[maybe_unused]] driver_id id, [[maybe_unused]] std::string_view name) {
        log<PANIC>("Exiting server...\n");
        services::get<Server>()->exit_all = true;
    };

    if (services::provide<rt::DriverManager>(on_fail)->init() < 0) {
        std::exit(EXIT_FAILURE);
    }

    if (services::provide<rt::SignalHandler>()->init() < 0) {
        std::exit(EXIT_FAILURE);
    }

    // ---------- Server services ----------

    services::provide<DriverExecutor>();
    services::provide<SessionManager>();
    services::provide<Server>()->run();

    std::exit(EXIT_SUCCESS);
    return 0;
}
