
#ifndef __EXECUTOR_HPP__
#define __EXECUTOR_HPP__

#include <array>
#include <memory>

namespace net { class Command; }

namespace koheron {

class DriverAbstract;
class DriverContainer;

class Executor {
public:
    Executor();

    ~Executor();
    Executor(Executor&&) noexcept;
    Executor& operator=(Executor&&) noexcept;
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

    int execute(net::Command& cmd);

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

} // namespace koheron

#endif // __EXECUTOR_HPP__
