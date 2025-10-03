/// (c) Koheron

#ifndef __CONTEXT_HPP__
#define __CONTEXT_HPP__

#include "server/runtime/syslog.hpp"

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

class Context {
  public:
    Context()
    : mm()
    , spi()
    , i2c()
    , fclk()
    , fpga()
    , cfg()
    {
        if (fpga.load_bitstream() < 0) {
           log<PANIC>("Failed to load bitstream. Exiting server...\n");
           exit(EXIT_FAILURE);
        }

        // We set all the Zynq clocks before starting the drivers
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

    MemoryManager mm;
    SpiManager spi;
    I2cManager i2c;
    ZynqFclk fclk;
    FpgaManager fpga;
    ConfigManager cfg;
};

#endif // __CONTEXT_HPP__
