#ifndef __DRIVERS_CONFIG_HPP__
#define __DRIVERS_CONFIG_HPP__

#include "drivers_table.hpp"

#include <drivers_list.hpp> // Provides driver_list

namespace drivers {

using table = koheron::drivers_table_t<driver_list>;

// Manual definition
// Do not include the generated drivers_list.hpp,
// simply list your drivers with forward declarations
// class Drv1;
// class Drv2;
// class Drv3;
// using table = koheron::drivers_table<Drv1, Drv2, Drv3>;

}// namespace drivers

#endif // __DRIVERS_CONFIG_HPP__