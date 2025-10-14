/// (c) Koheron

#ifndef __SERVER_CONTEXT_CONTEXT_HPP__
#define __SERVER_CONTEXT_CONTEXT_HPP__

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"

#include "server/hardware/memory_manager.hpp"
#include "server/hardware/spi_manager.hpp"
#include "server/hardware/i2c_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"
#include "server/hardware/fpga_manager.hpp"
#include "server/runtime/config_manager.hpp"

// Forward declarations

namespace rt {
template<class Driver> Driver& get_driver();
}

class Context {
  public:
    Context()
    : mm(services::require<hw::MemoryManager>())
    , spi(services::require<hw::SpiManager>())
    , i2c(services::require<hw::I2cManager>())
    , fclk(services::require<hw::ZynqFclk>())
    , fpga(services::require<hw::FpgaManager>())
    , cfg(services::require<rt::ConfigManager>())
    {}

    template<class Driver>
    Driver& get() const {
        return rt::get_driver<Driver>();
    }

    template<int severity=INFO, typename... Args>
    void log(const char *msg, Args&&... args) {
        rt::print<severity>(msg, std::forward<Args>(args)...);
    }

    template<int severity=INFO, typename... Args>
    void logf(std::format_string<Args...> fmt, Args&&... args) {
        rt::print_fmt<severity>(fmt, std::forward<Args>(args)...);
    }

    hw::MemoryManager& mm;
    hw::SpiManager&    spi;
    hw::I2cManager&    i2c;
    hw::ZynqFclk&      fclk;
    hw::FpgaManager&   fpga;
    rt::ConfigManager& cfg;
};

#endif // __SERVER_CONTEXT_CONTEXT_HPP__
