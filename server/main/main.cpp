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
#include "server/runtime/systemd.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/runtime/config_manager.hpp"

#include "server/core/session_manager.hpp"
#include "server/core/listener_manager.hpp"
#include "server/core/drivers/driver_executor.hpp"
#include <drivers.hpp> // For call to Common

#include <atomic>
#include <cstdlib>

using namespace koheron;
using services::provide;
using services::get;

int main() {
    std::atomic<bool> exit_all = false;

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
    auto on_fail = [&]([[maybe_unused]] driver_id id, [[maybe_unused]] std::string_view name) {
        exit_all = true;
    };

    auto dm = provide<rt::DriverManager>(on_fail);

    // If there is a Common driver with an init() method we call it
    if constexpr (drivers::table::has_driver<Common>) {
        if constexpr (rt::HasInit<Common>) {
            dm->get<Common>().init();
        }
    }

    auto signal_handler = rt::SignalHandler();

    if (signal_handler.init() < 0) {
        std::exit(EXIT_FAILURE);
    }

    // ---------- Server services ----------

    provide<DriverExecutor>();
    provide<SessionManager>();

    auto lm = ListenerManager();

    if (lm.start() < 0) {
        std::exit(EXIT_FAILURE);
    }

    bool ready_notified = false;

    while (true) {
        if (!ready_notified && lm.is_ready()) {
            log("Koheron server ready\n");

            if constexpr (config::notify_systemd) {
                rt::systemd::notify_ready("Koheron server is ready");
            }

            ready_notified = true;
        }

        if (signal_handler.interrupt() || exit_all) {
            log("Interrupt received, killing Koheron server ...\n");
            lm.shutdown();
            return 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::exit(EXIT_SUCCESS);
    return 0;
}
