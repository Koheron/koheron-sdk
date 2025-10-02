/// Server drivers manager
///
/// (c) Koheron

#ifndef __DRIVERS_MANAGER_HPP__
#define __DRIVERS_MANAGER_HPP__

#include <array>
#include <mutex>
#include <functional>

#include "server/runtime/services.hpp"

#ifdef KOHERON_SERVER_BUILD
  #include "server/core/configs/drivers_config.hpp"
#else
  #include <drivers_config.hpp>
#endif

namespace koheron {

class DriverContainer
{
  public:
    DriverContainer();
    ~DriverContainer();
    int init();

    template<driver_id driver>
    auto& get() {
        return *std::get<driver - drivers::table::offset>(driver_tuple);
    }

    template<driver_id driver>
    int alloc();

  private:
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

    explicit DriverManager(alloc_fail_cb on_alloc_fail = {});
    ~DriverManager();

    int init();

    template<driver_id id>
    auto& get() {
        const bool is_core_started = std::get<id - drivers::table::offset>(is_started);

        if (!is_core_started) {
            alloc_core(id);
        }

        return driver_container.get<id>();
    }

    template<class Driver>
    auto& get() {
        return get<drivers::table::id_of<Driver>>();
    }

  private:
    DriverContainer driver_container;
    std::array<bool, drivers::table::size - drivers::table::offset> is_started{};
    std::recursive_mutex mutex;
    alloc_fail_cb on_alloc_fail_;

    template<driver_id id>
    void alloc_core_();

    void alloc_core(driver_id id);
};

// Driver access

template<class Driver>
Driver& get_driver() {
    return services::require<DriverManager>().template get<Driver>();
}

} // namespace koheron

#endif // __DRIVERS_MANAGER_HPP__
