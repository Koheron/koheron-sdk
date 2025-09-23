/// Server drivers manager
///
/// (c) Koheron

#ifndef __DRIVERS_MANAGER_HPP__
#define __DRIVERS_MANAGER_HPP__

#include <array>
#include <memory>
#include <cassert>
#include <thread>
#include <mutex>
#include <functional>

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"

#ifdef KOHERON_SERVER_BUILD
  #include "server/core/configs/drivers_config.hpp"
#else
  #include <drivers_config.hpp>
#endif

#include <drivers.hpp> // Drivers includes

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
        return *std::get<driver - drivers::table::offset>(driver_tuple);
    }

    auto& context() { return ctx; }

    template<driver_id driver>
    int alloc();

  private:
    Context ctx;

    std::array<bool, drivers::table::size - drivers::table::offset> is_started;
    std::array<bool, drivers::table::size - drivers::table::offset> is_starting;

    using drivers_tuple_t = typename drivers::table::tuple_t;
    drivers_tuple_t driver_tuple;

    template<driver_id driver>
    bool is_driver_started() {
        return std::get<driver - drivers::table::offset>(is_started);
    }
};

class DriverManager
{
  public:
    using alloc_fail_cb = std::function<void(driver_id, std::string_view)>;

    explicit DriverManager(alloc_fail_cb on_alloc_fail = {})
    : driver_container()
    , on_alloc_fail_(std::move(on_alloc_fail))
    {
        is_started.fill(false);
    }

    int init();

    template<driver_id id>
    auto& get() {
        ensure_core_started(id);
        return driver_container.get<id>();
    }

    template<class Driver>
    auto& get() {
        return get<drivers::table::id_of<Driver>>();
    }

    auto& context() {
        return driver_container.context();
    }

    // internal (used by executor via public API)
    bool is_core_started(driver_id id) const {
        return is_started[id - drivers::table::offset];
    }

  private:
    DriverContainer driver_container;
    std::array<bool, drivers::table::size - drivers::table::offset> is_started{};
    std::recursive_mutex mutex;
    alloc_fail_cb on_alloc_fail_;

    void ensure_core_started(driver_id id);

    template<driver_id id>
    void alloc_core_();

    void alloc_core(driver_id id);
};

// Driver access
// We require that a DriverManager service to be provided

template<class Driver>
Driver& get_driver() {
    return services::require<DriverManager>().template get<Driver>();;
}

} // namespace koheron

#endif // __DRIVERS_MANAGER_HPP__
