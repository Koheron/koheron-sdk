#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/systemd.hpp"
#include "server/runtime/signal_handler.hpp"
#include "server/runtime/executor.hpp"

#include "server/network/listener_manager.hpp"
#include "server/network/session_manager.hpp"
#include "server/network/commands.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <string>

using services::provide;
using services::require;

class AppExecutor final : public rt::IExecutor {
  public:
    int handle_app(net::Command& cmd) override {
        if (cmd.driver != 0xFFFF) {
            return 0;
        }

        if (cmd.operation == 0) {
            std::string s;
            cmd.read_one(s);
            return cmd.send(s);
        }

        return 0;
    }
};

int main() {
    rt::SignalHandler signal_handler;
    if (signal_handler.init() < 0) std::exit(EXIT_FAILURE);

    rt::provide_executor<AppExecutor>();

    net::ListenerManager lm;
    if (lm.start() < 0) std::exit(EXIT_FAILURE);

    bool ready_notified = false;

    for (;;) {
        if (!ready_notified && lm.is_ready()) {
            log("app is ready\n");
            if constexpr (net::config::notify_systemd) {
                rt::systemd::notify_ready("FFT app is ready");
            }
            ready_notified = true;
        }

        if (signal_handler.interrupt()) {
            log("Interrupt received, shutting down...\n");
            lm.shutdown();
            return 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}