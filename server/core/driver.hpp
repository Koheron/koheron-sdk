/// Server drivers
///
/// (c) Koheron

#ifndef __DRIVER_HPP__
#define __DRIVER_HPP__

#include <cstring>

#include "server_definitions.hpp"
#include <drivers_table.hpp>

namespace koheron {

class Server;
struct Command;

class DriverAbstract {
  public:
    DriverAbstract(driver_id type_, Server *server_)
    : type(type_)
    , server(server_)
    {}

    driver_id type = driver_id_of<NoDriver>;
    Server *server;
friend class DriverManager;
};

template<driver_id type>
class Driver : public DriverAbstract {};

} // namespace koheron

#endif // __DRIVER_HPP__
