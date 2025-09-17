/// Implementation of drivers_manager.hpp
///
/// (c) Koheron

#include "drivers_manager.hpp"
#include "server.hpp"
#include "commands.hpp"
#include "meta_utils.hpp"
#include "driver_id.hpp"
#include <interface_drivers.hpp>

namespace koheron {

//----------------------------------------------------------------------------
// Driver container
//----------------------------------------------------------------------------

template<driver_id driver>
int DriverContainer::alloc() {
    if (is_driver_started<driver>()) {
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

template<driver_id driver>
void DriverManager::alloc_driver()
{
    std::scoped_lock lock(mutex);

    if (is_driver_started<driver>()) {
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

template<driver_id... drivers>
void DriverManager::start(driver_id driver, std::index_sequence<drivers...>)
{
    ((driver == drivers ? (alloc_driver<drivers>(), void()) : void()), ...);
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

template<driver_id... drivers>
int DriverManager::execute_driver(DriverAbstract *dev_abs, Command& cmd,
                                  std::index_sequence<drivers...>)
{
    int result = 0;
    bool done = false;

    // Left-to-right evaluation; only the matching branch runs work.
    (void)std::initializer_list<int>{
        ((dev_abs->type == drivers && !done)
            ? (result = static_cast<Driver<drivers>*>(dev_abs)->execute(cmd),
               done = true, 0)
            : 0)...
    };

    return result;
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
