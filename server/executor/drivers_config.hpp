#ifndef __DRIVERS_CONFIG_HPP__
#define __DRIVERS_CONFIG_HPP__

#include <drivers_list.hpp> // Provides driver_list

namespace drivers {

struct NoDriver;
struct Server;

using table = rt::drivers_table_t<std::tuple<NoDriver, Server>, driver_list>;

} // namespace drivers

#endif // __DRIVERS_CONFIG_HPP__

