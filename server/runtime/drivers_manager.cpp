/// Implementation of drivers_manager.hpp
///
/// (c) Koheron

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/drivers_manager.hpp"
#include "server/utilities/meta_utils.hpp"
#include "server/context/context.hpp"

#include <drivers.hpp> // Drivers includes

namespace rt {

//----------------------------------------------------------------------------
// Driver container
//----------------------------------------------------------------------------

template<class D>
inline constexpr bool needs_context_v =
    std::is_constructible_v<D, Context&> ||
    std::is_constructible_v<D, const Context&>;

template <class D>
[[deprecated("Driver constructors taking Context& are deprecated; "
             "prefer a default constructor and use services inside the driver.")]]
inline Context& deprecated_context_arg_for() {
    return services::require<Context>();
}

template<class D>
std::unique_ptr<D> make_driver()
{
    static_assert(
        std::is_default_constructible_v<D> || needs_context_v<D>,
        "Driver must be default-constructible or constructible from Context&");

    if constexpr (std::is_default_constructible_v<D>) {
        if constexpr (needs_context_v<D>) {
            (void)&deprecated_context_arg_for<D>;
        }
        return std::make_unique<D>();
    } else {
        return std::make_unique<D>(deprecated_context_arg_for<D>());
    }
}

DriverContainer::DriverContainer() {
    is_started.fill(false);
    is_starting.fill(false);
    services::provide<Context>();
}

DriverContainer::~DriverContainer() = default;

template<driver_id driver>
int DriverContainer::alloc() {
    if (is_driver_started<driver>()) {
        return 0;
    }

    constexpr auto id = driver - drivers::table::offset;

    if (std::get<id>(is_starting)) {
        logf<CRITICAL>(
            "Circular dependency detected while initializing driver [{}] {}\n",
            driver, std::get<driver>(drivers::table::names));

        return -1;
    }

    std::get<id>(is_starting) = true;
    std::get<id>(driver_tuple) = make_driver<drivers::table::type_of<driver>>();;
    std::get<id>(is_starting) = false;
    std::get<id>(is_started) = true;

    return 0;
}

//----------------------------------------------------------------------------
// Driver manager
//----------------------------------------------------------------------------

DriverManager::DriverManager(alloc_fail_cb on_alloc_fail)
: driver_container()
, on_alloc_fail_(std::move(on_alloc_fail))
{
    is_started.fill(false);
}

int DriverManager::init() {
    // If there is a Common driver with an init() method we call it
    if constexpr (drivers::table::has_driver<Common>) {
        if constexpr (HasInit<Common>) {
            get<Common>().init();
        }
    }

    return 0;
}

DriverManager::~DriverManager() = default;

template<driver_id id>
void DriverManager::alloc_core_() {
    std::scoped_lock lock(mutex);

    if (std::get<id - drivers::table::offset>(is_started)) {
        return;
    }

    const auto driver_name = std::get<id>(drivers::table::names);

    logf("Driver Manager: Allocating driver [{}] {}...\n", id, driver_name);

    if (driver_container.alloc<id>() < 0) {
        logf<PANIC>("Driver Manager: Failed to allocate driver [{}] {}.\n", id, driver_name);

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

} // namespace rt
