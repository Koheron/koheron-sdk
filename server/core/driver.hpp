/// Server drivers
///
/// (c) Koheron

#ifndef __DRIVER_HPP__
#define __DRIVER_HPP__

#include <cstring>

#include "server_definitions.hpp"
#include "drivers_table.hpp"

namespace koheron {

struct Command;

class DriverAbstract {
  public:
    DriverAbstract(driver_id type_)
    : type(type_)
    {}

    driver_id type = driver_table::id_of<NoDriver>;
};

template<driver_id type>
class Driver : public DriverAbstract {};

} // namespace koheron

#endif // __DRIVER_HPP__
