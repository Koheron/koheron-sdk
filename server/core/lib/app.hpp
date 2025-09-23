#include "syslog.hpp"
#include "services.hpp"
#include "drivers_manager.hpp"

#include <memory>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cstddef>

namespace koheron {

class App {
  public:
    App() {
        start_syslog();
        dm_ = services::provide<DriverManager>(on_fail_);
        if (dm_->init() < 0) {
            std::exit(EXIT_FAILURE);
        }
    }

    ~App() {
        stop_syslog();
    }

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    // Access to the Context
    auto& context() { return dm_->context(); }
    const auto& context() const { return dm_->context(); }

    // Convenience so you can write: app->log<...>(...)
    auto* operator->() { return &dm_->context(); }
    const auto* operator->() const { return &dm_->context(); }

    void notify_systemd_ready() {
        const char* sock = std::getenv("NOTIFY_SOCKET");

        if (!sock || !*sock) {
            return;
        }

        int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);

        if (fd < 0) {
            return;
        }

        sockaddr_un sa{};
        sa.sun_family = AF_UNIX;
        std::size_t len = std::strlen(sock);

        if (sock[0] == '@') {
            sa.sun_path[0] = '\0';

            if (len > 1) {
                std::memcpy(sa.sun_path + 1, sock + 1, len - 1);
            }
        } else {
            std::strncpy(sa.sun_path, sock, sizeof(sa.sun_path) - 1);
            len += 1;
        }

        socklen_t addrlen = offsetof(sockaddr_un, sun_path) + len;

        static constexpr char msg[] = "READY=1";
        (void)::sendto(fd, msg, sizeof(msg) - 1, 0, reinterpret_cast<sockaddr*>(&sa), addrlen);

        ::close(fd);
    }

  private:
    static void on_fail_(driver_id id, std::string_view name) {
        print_fmt<PANIC>("DriverManager: driver [{}] {} failed to allocate.\n", id, name);
        std::exit(EXIT_FAILURE);
    }

    std::shared_ptr<DriverManager> dm_;
};

} // namespace koheron