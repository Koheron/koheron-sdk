/// Implementation of drivers_manager.hpp
///
/// (c) Koheron

#include "server/runtime/drivers_manager.hpp"
#include "server/runtime/meta_utils.hpp"

namespace koheron {

//----------------------------------------------------------------------------
// Driver container
//----------------------------------------------------------------------------

template<driver_id driver>
int DriverContainer::alloc() {
    if (is_driver_started<driver>()) {
        return 0;
    }

    constexpr auto id = driver - drivers::table::offset;

    if (std::get<id>(is_starting)) {
        print_fmt<CRITICAL>(
            "Circular dependency detected while initializing driver [{}] {}\n",
            driver, std::get<driver>(drivers::table::names));

        return -1;
    }

    std::get<id>(is_starting) = true;
    std::get<id>(driver_tuple) = std::make_unique<drivers::table::type_of<driver>>(services::require<Context>());
    std::get<id>(is_starting) = false;
    std::get<id>(is_started) = true;

    return 0;
}

//----------------------------------------------------------------------------
// Driver manager
//----------------------------------------------------------------------------

int DriverManager::init()
{
    if (driver_container.init() < 0) {
        return -1;
    }

    return 0;
}

template<driver_id id>
void DriverManager::alloc_core_() {
    std::scoped_lock lock(mutex);

    if (std::get<id - drivers::table::offset>(is_started)) {
        return;
    }

    const auto driver_name = std::get<id>(drivers::table::names);

    print_fmt<INFO>("Driver Manager: Allocating driver [{}] {}...\n", id, driver_name);

    if (driver_container.alloc<id>() < 0) {
        print_fmt<PANIC>("Driver Manager: Failed to allocate driver [{}] {}.\n", id, driver_name);

        if (on_alloc_fail_) {
            on_alloc_fail_(id, driver_name);
        }
        return;
    }

    std::get<id - drivers::table::offset>(is_started) = true;
}

void DriverManager::alloc_core(driver_id id) {
    // runtime switch via fold
    auto seq = make_index_sequence_in_range<drivers::table::offset, drivers::table::size>();
    (void)seq;

    auto do_alloc = [this, id]<driver_id... ids>(std::index_sequence<ids...>) {
        ((id == ids ? (alloc_core_<ids>(), void()) : void()), ...);
    };

    do_alloc(seq);
}

} // namespace koheron
