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
#include <drivers_table.hpp>

namespace koheron {

class DriverContainer
{
  public:
    DriverContainer(Context& ctx_, SysLog& syslog_)
    : ctx(ctx_)
    , syslog(syslog_)
    {
        is_started.fill(false);
        is_starting.fill(false);
    }

    template<driver_id driver>
    auto& get() {
        return *std::get<driver - 2>(driver_tuple);
    }

    template<driver_id driver>
    int alloc();

  private:
    Context& ctx;
    SysLog& syslog;

    std::array<bool, device_num - 2> is_started;
    std::array<bool, device_num - 2> is_starting;

    drivers_tuple_t driver_tuple;
};

class Server;
struct Command;

class DriverManager
{
  public:
    explicit DriverManager(Server *server_);

    int init();
    int execute(Command &cmd);

    template<driver_id driver>
    auto& get() {
        if (! std::get<driver - 2>(is_started)) {
            alloc_driver<driver>();
        }
        return driver_container.get<driver>();
    }

  private:
    // Store drivers (except Server) as unique_ptr
    std::array<std::unique_ptr<DriverAbstract>, device_num - 2> device_list;
    Server *server;
    DriverContainer driver_container;
    std::array<bool, device_num - 2> is_started;
    std::recursive_mutex mutex;

    Context ctx;

    template<std::size_t driver> void alloc_driver();

    // Start

    template<driver_id... drivers>
    void start(driver_id driver, std::index_sequence<drivers...>);

    template<driver_id dev0, driver_id... drivers>
    std::enable_if_t<0 == sizeof...(drivers) && 2 <= dev0, void>
    start_impl(driver_id driver);

    template<driver_id dev0, driver_id... drivers>
    std::enable_if_t<0 < sizeof...(drivers) && 2 <= dev0, void>
    start_impl(driver_id driver);

    // Execute

    template<driver_id... drivers>
    int execute_driver(DriverAbstract *dev_abs, Command& cmd, std::index_sequence<drivers...>);

    template<driver_id dev0, driver_id... drivers>
    std::enable_if_t<0 == sizeof...(drivers) && 2 <= dev0, int>
    execute_driver_implementation(DriverAbstract *dev_abs, Command& cmd);

    template<driver_id dev0, driver_id... drivers>
    std::enable_if_t<0 < sizeof...(drivers) && 2 <= dev0, int>
    execute_driver_implementation(DriverAbstract *dev_abs, Command& cmd);
};

} // namespace koheron

#endif // __DRIVERS_MANAGER_HPP__
