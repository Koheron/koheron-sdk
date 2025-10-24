/// Main file for koheron-server
///
/// (c) Koheron

#include "server/hardware/memory_manager.hpp"
#include "server/hardware/spi_manager.hpp"
#include "server/hardware/i2c_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"
#include "server/hardware/fpga_manager.hpp"
#if defined(KOHERON_HAS_REMOTEPROC_MANAGER)
#include "server/hardware/remoteproc_manager.hpp"
#include <filesystem>
#endif

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

#if defined(KOHERON_HAS_REMOTEPROC_MANAGER)
    const auto live_root = std::filesystem::path{"/tmp/live-instrument"};
    const auto firmware_manifest = live_root / "firmware.manifest";
    const auto remoteproc_manifest = live_root / "remoteproc.manifest";
    const bool have_firmware = std::filesystem::exists(firmware_manifest);
    const bool have_remoteproc = std::filesystem::exists(remoteproc_manifest);

    if (have_firmware || have_remoteproc) {
        auto rpu = provide<hw::RpuFirmwareManager>();
        if (rpu->install_all() < 0) {
            log<ERROR>("Failed to install Cortex-R5 firmware\n");
        }
        if (have_remoteproc && rpu->start_all() < 0) {
            log<ERROR>("Failed to start Cortex-R5 firmware\n");
        }
    }
#endif

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
