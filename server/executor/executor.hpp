
#ifndef __EXECUTOR_HPP__
#define __EXECUTOR_HPP__

#include <array>
#include <memory>

namespace koheron {

struct Command;
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

    int execute(Command& cmd);

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

} // namespace koheron

#endif // __EXECUTOR_HPP__
