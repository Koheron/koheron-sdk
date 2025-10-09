/// (c) Koheron

#ifndef __SERVER_CONTEXT_CONTEXT_HPP__
#define __SERVER_CONTEXT_CONTEXT_HPP__

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"

#include "server/context/memory_manager.hpp"
#include "server/context/spi_dev.hpp"
#include "server/context/i2c_dev.hpp"
#include "server/context/zynq_fclk.hpp"
#include "server/context/fpga_manager.hpp"
#include "server/context/config_manager.hpp"

// Forward declarations

namespace koheron {
template<class Driver> Driver& get_driver();
}

inline void provide_context_services() {
    if (!services::get<MemoryManager>()) {
        services::provide<MemoryManager>();
        services::provide<SpiManager>();
        services::provide<I2cManager>();
        services::provide<ZynqFclk>();
        services::provide<FpgaManager>();
        services::provide<ConfigManager>();
    }
}

class Context {
  public:
    Context()
    : mm(services::require<MemoryManager>())
    , spi(services::require<SpiManager>())
    , i2c(services::require<I2cManager>())
    , fclk(services::require<ZynqFclk>())
    , fpga(services::require<FpgaManager>())
    , cfg(services::require<ConfigManager>())
    {
        if (fpga.load_bitstream() < 0) {
            log<PANIC>("Failed to load bitstream. Exiting server...\n");
            std::exit(EXIT_FAILURE);
        }

        // Set Zynq clocks before starting drivers
        zynq_clocks::set_clocks(fclk);
    }

    int init() {
        if (mm.open() < 0  ||
            spi.init() < 0 ||
            i2c.init() < 0 ||
            cfg.init() < 0)
            return -1;
        return 0;
    }

    template<class Driver>
    Driver& get() const {
        return koheron::get_driver<Driver>();
    }

    template<int severity, typename... Args>
    void log(const char *msg, Args&&... args) {
        koheron::print<severity>(msg, std::forward<Args>(args)...);
    }

    template<int severity, typename... Args>
    void logf(std::format_string<Args...> fmt, Args&&... args) {
        koheron::print_fmt<severity>(fmt, std::forward<Args>(args)...);
    }

    MemoryManager& mm;
    SpiManager&    spi;
    I2cManager&    i2c;
    ZynqFclk&      fclk;
    FpgaManager&   fpga;
    ConfigManager& cfg;
};

#endif // __SERVER_CONTEXT_CONTEXT_HPP__
