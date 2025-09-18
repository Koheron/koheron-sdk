
#ifndef __DRIVER_EXECUTOR_HPP__
#define __DRIVER_EXECUTOR_HPP__

#include <array>
#include <memory>
#include "driver_id.hpp"

namespace koheron {

struct Command;
class DriverAbstract;
class DriverContainer;

class DriverExecutor {
public:
     DriverExecutor();

    ~DriverExecutor();
    DriverExecutor(DriverExecutor&&) noexcept;
    DriverExecutor& operator=(DriverExecutor&&) noexcept;
    DriverExecutor(const DriverExecutor&) = delete;
    DriverExecutor& operator=(const DriverExecutor&) = delete;

    int execute(Command& cmd);

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

} // namespace koheron

#endif // __DRIVER_EXECUTOR_HPP__