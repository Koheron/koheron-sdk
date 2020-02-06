/// Implementation of drivers_manager.hpp
///
/// (c) Koheron

#include "drivers_manager.hpp"
#include "server.hpp"
#include "commands.hpp"
#include "meta_utils.hpp"
#include <interface_drivers.hpp>

namespace koheron {

//----------------------------------------------------------------------------
// Driver container
//----------------------------------------------------------------------------

template<driver_id driver>
int DriverContainer::alloc() {
    if (std::get<driver - 2>(is_started)) {
        return 0;
    }

    if (std::get<driver - 2>(is_starting)) {
        syslog.print<CRITICAL>(
            "Circular dependency detected while initializing driver [%u] %s\n",
            driver, std::get<driver>(drivers_names).data());

        return -1;
    }

    std::get<driver - 2>(is_starting) = true;
    std::get<driver - 2>(driver_tuple) = std::make_unique<device_t<driver>>(ctx);
    std::get<driver - 2>(is_starting) = false;
    std::get<driver - 2>(is_started) = true;

    return 0;
}

//----------------------------------------------------------------------------
// Driver manager
//----------------------------------------------------------------------------

DriverManager::DriverManager(Server *server_)
: server(server_)
, driver_container(ctx, server->syslog)
{
    ctx.set_driver_manager(this);
    ctx.set_syslog(&server->syslog);
    is_started.fill(false);
}

template<std::size_t driver>
void DriverManager::alloc_driver()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (std::get<driver - 2>(is_started)) {
        return;
    }

    server->syslog.print<INFO>(
        "Driver Manager: Starting driver [%u] %s...\n",
        driver, std::get<driver>(drivers_names).data());

    if (driver_container.alloc<driver>() < 0) {
        server->syslog.print<PANIC>(
            "Failed to allocate driver [%u] %s. Exiting server...\n",
            driver, std::get<driver>(drivers_names).data());

        server->exit_all = true;
        return;
    }

    std::get<driver - 2>(device_list)
        = std::make_unique<Driver<driver>>(server, driver_container.get<driver>());
    std::get<driver - 2>(is_started) = true;
}

template<driver_id dev0, driver_id... drivers>
std::enable_if_t<0 == sizeof...(drivers) && 2 <= dev0, void>
DriverManager::start_impl(driver_id)
{
    static_assert(dev0 < device_num, "");
    static_assert(dev0 >= 2, "");
    alloc_driver<dev0>();
}

template<driver_id dev0, driver_id... drivers>
std::enable_if_t<0 < sizeof...(drivers) && 2 <= dev0, void>
DriverManager::start_impl(driver_id driver)
{
    static_assert(dev0 < device_num, "");
    static_assert(dev0 >= 2, "");

    driver == dev0 ? alloc_driver<dev0>()
                : start_impl<drivers...>(driver);
}

template<driver_id... drivers>
void DriverManager::start(driver_id driver, std::index_sequence<drivers...>)
{
    start_impl<drivers...>(driver);
}

int DriverManager::init()
{
    if (ctx.init() < 0) {
        server->syslog.print<CRITICAL>("Context initialization failed\n");
        return -1;
    }

    get<driver_id_of<Common>>().init();
    return 0;
}

template<driver_id dev0, driver_id... drivers>
std::enable_if_t<0 == sizeof...(drivers) && 2 <= dev0, int>
DriverManager::execute_driver_implementation(DriverAbstract *dev_abs, Command& cmd)
{
    static_assert(dev0 < device_num, "");
    static_assert(dev0 >= 2, "");
    return static_cast<Driver<dev0>*>(dev_abs)->execute(cmd);
}

template<driver_id dev0, driver_id... drivers>
std::enable_if_t<0 < sizeof...(drivers) && 2 <= dev0, int>
DriverManager::execute_driver_implementation(DriverAbstract *dev_abs, Command& cmd)
{
    static_assert(dev0 < device_num, "");
    static_assert(dev0 >= 2, "");

    return dev_abs->type == dev0 ? static_cast<Driver<dev0>*>(dev_abs)->execute(cmd)
                                 : execute_driver_implementation<drivers...>(dev_abs, cmd);
}

template<driver_id... drivers>
int DriverManager::execute_driver(DriverAbstract *dev_abs, Command& cmd,
                               std::index_sequence<drivers...>)
{
    static_assert(sizeof...(drivers) == device_num - 2, "");
    return execute_driver_implementation<drivers...>(dev_abs, cmd);
}

int DriverManager::execute(Command& cmd)
{
    assert(cmd.driver < device_num);

    if (cmd.driver == 0) {
        return 0;
    }

    if (cmd.driver == 1) {
        return server->execute(cmd);
    }

    if (!is_started[cmd.driver - 2]) {
        start(cmd.driver, make_index_sequence_in_range<2, device_num>());
    }

    return execute_driver(device_list[cmd.driver - 2].get(), cmd,
                       make_index_sequence_in_range<2, device_num>());
}

} // namespace koheron
