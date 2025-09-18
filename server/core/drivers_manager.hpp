/// Server drivers manager
///
/// (c) Koheron

#ifndef __DRIVERS_MANAGER_HPP__
#define __DRIVERS_MANAGER_HPP__

#include "config.hpp"

#include <array>
#include <memory>
#include <cassert>
#include <thread>
#include <mutex>

#include "driver.hpp"
#include <drivers.hpp>
#include "services.hpp"
#include "lib/syslog.hpp"

namespace koheron {

class DriverContainer
{
  public:
    DriverContainer()
    : ctx()
    {
        is_started.fill(false);
        is_starting.fill(false);
    }

    int init()
    {
        if (ctx.init() < 0) {
            print<CRITICAL>("Context initialization failed\n");
            return -1;
        }

        return 0;
    }

    template<driver_id driver>
    auto& get() {
        return *std::get<driver - 2>(driver_tuple);
    }

    template<driver_id driver>
    int alloc();

  private:
    Context ctx;

    std::array<bool, device_num - 2> is_started;
    std::array<bool, device_num - 2> is_starting;

    drivers_tuple_t driver_tuple;

    template<driver_id driver>
    bool is_driver_started() {
        return std::get<driver - 2>(is_started);
    }
};

class DriverManager
{
  public:
    DriverManager();

    int init();
    void ensure_core_started(driver_id id);

    // Typed access to the core driver
    template<driver_id id>
    auto& core() {
        ensure_core_started(id);
        return driver_container.get<id>();
    }

    template<driver_id id>
    auto& get() { return core<id>(); }

    // internal (used by executor via public API)
    bool is_core_started(driver_id id) const {
        return is_started[id - 2];
    }

  private:
    DriverContainer driver_container;
    std::array<bool, device_num - 2> is_started{};
    std::recursive_mutex mutex;

    template<driver_id id>
    void alloc_core_();

    void alloc_core(driver_id id);
};

template<driver_id id>
device_t<id>& get_driver() {
    return services::require<koheron::DriverManager>().template core<id>();
}

} // namespace koheron

#endif // __DRIVERS_MANAGER_HPP__
