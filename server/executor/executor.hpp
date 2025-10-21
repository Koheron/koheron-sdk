
#ifndef __EXECUTOR_HPP__
#define __EXECUTOR_HPP__

#include "server/runtime/executor.hpp"

#include <array>
#include <memory>

namespace net { class Command; }

namespace koheron {

class DriverAbstract;
class DriverContainer;

class Executor final : public rt::IExecutor {
  public:
    Executor();
    ~Executor() override;

    Executor(Executor&&) noexcept;
    Executor& operator=(Executor&&) noexcept;
    // No copy
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

  private:
    int handle_app(net::Command& cmd) override;

    struct Impl;
    std::unique_ptr<Impl> p_;
};

} // namespace koheron

#endif // __EXECUTOR_HPP__
