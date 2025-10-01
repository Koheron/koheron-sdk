/// Server drivers
///
/// (c) Koheron

#ifndef __DRIVER_HPP__
#define __DRIVER_HPP__

#include "server/runtime/drivers_table.hpp"

#include <cstring>

namespace koheron {

struct Command;

class DriverAbstract {
  public:
    DriverAbstract(driver_id type_)
    : type(type_)
    {}

    driver_id type = 0;
};

template<driver_id type>
class Driver : public DriverAbstract {};

} // namespace koheron

#endif // __DRIVER_HPP__
