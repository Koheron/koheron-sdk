/// Implementation of drivers_manager.hpp
///
/// (c) Koheron

#include "drivers_manager.hpp"
#include "server.hpp"
#include "meta_utils.hpp"
#include "drivers_table.hpp"
#include "services.hpp"

namespace koheron {

//----------------------------------------------------------------------------
// Driver container
//----------------------------------------------------------------------------

template<driver_id driver>
int DriverContainer::alloc() {
    if (is_driver_started<driver>()) {
        return 0;
    }

    if (std::get<driver - driver_table::offset>(is_starting)) {
        print<CRITICAL>(
            "Circular dependency detected while initializing driver [%u] %s\n",
            driver, std::get<driver>(driver_table::names).data());

        return -1;
    }

    std::get<driver - driver_table::offset>(is_starting) = true;
    std::get<driver - driver_table::offset>(driver_tuple) = std::make_unique<driver_table::type_of<driver>>(ctx);
    std::get<driver - driver_table::offset>(is_starting) = false;
    std::get<driver - driver_table::offset>(is_started) = true;

    return 0;
}

//----------------------------------------------------------------------------
// Driver manager
//----------------------------------------------------------------------------

DriverManager::DriverManager()
: driver_container()
{
    is_started.fill(false);
}

int DriverManager::init()
{
    if (driver_container.init() < 0) {
        return -1;
    }

    core<driver_table::id_of<Common>>().init(); // Maybe we don't want the hardwired ?
    return 0;
}

template<driver_id id>
void DriverManager::alloc_core_() {
    std::scoped_lock lock(mutex);

    if (is_started[id - driver_table::offset]) {
        return;
    }

    print_fmt<INFO>("Driver Manager: Allocating driver [{}] {}...\n",
                    id, std::get<id>(driver_table::names));

    if (driver_container.alloc<id>() < 0) {
        print_fmt<PANIC>("Failed to allocate driver [{}] {}. Exiting server...\n",
                         id, std::get<id>(driver_table::names));
        services::require<Server>().exit_all = true;
        return;
    }

    is_started[id - driver_table::offset] = true;
}

void DriverManager::alloc_core(driver_id id) {
    // runtime switch via fold
    auto seq = make_index_sequence_in_range<driver_table::offset, driver_table::size>();
    (void)seq;

    auto do_alloc = [this, id]<driver_id... ids>(std::index_sequence<ids...>) {
        ((id == ids ? (alloc_core_<ids>(), void()) : void()), ...);
    };

    do_alloc(seq);
}

void DriverManager::ensure_core_started(driver_id id) {
    if (!is_core_started(id)) {
        alloc_core(id);
    }
}

} // namespace koheron
