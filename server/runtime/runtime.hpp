#include "syslog.hpp"
#include "services.hpp"
#include "drivers_manager.hpp"
#include "systemd.hpp"

namespace koheron {

class Runtime {
  public:
    Runtime() {
        start_syslog();
        dm_ = services::provide<DriverManager>(on_fail_);
        if (dm_->init() < 0) {
            std::exit(EXIT_FAILURE);
        }
    }

    ~Runtime() {
        stop_syslog();
    }

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    // Access to the Context
    auto& context() { return dm_->context(); }
    const auto& context() const { return dm_->context(); }

    // Convenience so you can write: rt->log<...>(...)
    auto* operator->() { return &dm_->context(); }
    const auto* operator->() const { return &dm_->context(); }

    void notify_systemd_ready() {
        systemd::notify_ready();
    }

  private:
    static void on_fail_(driver_id id, std::string_view name) {
        print_fmt<PANIC>("DriverManager: driver [{}] {} failed to allocate.\n", id, name);
        std::exit(EXIT_FAILURE);
    }

    std::shared_ptr<DriverManager> dm_;
};

} // namespace koheron