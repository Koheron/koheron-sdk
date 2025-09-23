#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/drivers_manager.hpp"
#include "server/runtime/systemd.hpp"

namespace koheron {

class Runtime {
  public:
    Runtime() {
        start_syslog();
        auto dm = services::provide<DriverManager>(on_fail_);

        if (dm->init() < 0) {
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
    auto& context() { return services::require<Context>(); }
    const auto& context() const { return services::require<Context>(); }

    void systemd_notify_ready() {
        systemd::notify_ready();
    }

  private:
    static void on_fail_(driver_id id, std::string_view name) {
        print_fmt<PANIC>("DriverManager: driver [{}] {} failed to allocate.\n", id, name);
        std::exit(EXIT_FAILURE);
    }
};

} // namespace koheron