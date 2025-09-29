/// Server drivers
///
/// (c) Koheron

#ifndef __DRIVER_HPP__
#define __DRIVER_HPP__

#include "server/core/configs/drivers_config.hpp"

#include <cstring>

namespace koheron {

struct Command;

class DriverAbstract {
  public:
    DriverAbstract(driver_id type_)
    : type(type_)
    {}

    driver_id type = drivers::table::id_of<drivers::NoDriver>;
};

template<driver_id type>
class Driver : public DriverAbstract {};

} // namespace koheron

#endif // __DRIVER_HPP__
