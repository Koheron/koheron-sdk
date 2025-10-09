#ifndef __SERVER_DRIVERS_UIO_HPP__
#define __SERVER_DRIVERS_UIO_HPP__

#include "server/runtime/syslog.hpp"
#include "server/context/memory_map.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <cstdint>
#include <climits>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <chrono>
#include <utility>

template <MemID uio_mem>
class Uio
{
  public:
    Uio() = default;

    // Non-copyable, movable (unique FD owner)
    Uio(const Uio&) = delete;
    Uio& operator=(const Uio&) = delete;
    Uio(Uio&& other) noexcept : fd(std::exchange(other.fd, -1)) {}

    Uio& operator=(Uio&& other) noexcept {
        if (this != &other) {
            close_fd();
            fd = std::exchange(other.fd, -1);
        }

        return *this;
    }

    ~Uio() { close_fd(); }

    int open() {
        namespace fs = std::filesystem;
        const fs::path uio_root{"/sys/class/uio"};
        std::error_code ec;

        if (!fs::exists(uio_root, ec) || !fs::is_directory(uio_root, ec)) {
            koheron::print_fmt<ERROR>("Uio: Cannot open {} [{}]\n",
                                      uio_root.string(),
                                      ec ? ec.message().c_str() : std::strerror(errno));
            return -1;
        }

        int new_fd = -1;

        for (const auto& entry : fs::directory_iterator(uio_root, ec)) {
            if (ec) {
                koheron::print_fmt<ERROR>("Uio: directory_iterator error: {}\n", ec.message());
                break;
            }

            if (!entry.is_directory()) {
                continue;
            }

            const auto name = entry.path().filename().string();
            if (!name.starts_with("uio")) {
                continue;
            }

            const auto addr_path = entry.path() / "maps/map0/addr";
            std::ifstream f(addr_path);
            if (!f) {
                continue;
            }

            uintptr_t addr = 0;
            if (!(f >> std::hex >> addr)) {
                continue;
            }

            if (addr == mem_base) {
                const auto dev_path = fs::path("/dev") / name;
                new_fd = ::open(dev_path.c_str(), O_RDWR | O_CLOEXEC);

                if (new_fd < 0) {
                    koheron::print_fmt<ERROR>("Uio: Cannot open {}: {}\n",
                                              dev_path, std::strerror(errno));
                } else {
                    koheron::print_fmt<INFO>("Uio: Using {} for IRQs (addr={:#x})\n",
                                             dev_path, mem_base);
                }

                break;
            }
        }

        if (new_fd < 0) {
            koheron::print_fmt<ERROR>("Uio: No device with base {:#x}\n", mem_base);
            return -1;
        }

        close_fd();
        fd = new_fd;
        return fd;
    }

    bool arm_irq() {
        if (fd < 0) {
            koheron::print_fmt<ERROR>("Uio: arm_irq on invalid fd\n");
            return false;
        }

        std::uint32_t arm = 1;
        ssize_t n;
        do {
            n = ::write(fd, &arm, sizeof(arm));
        } while (n < 0 && errno == EINTR);

        if (n != static_cast<ssize_t>(sizeof(arm))) {
            koheron::print_fmt<ERROR>("Uio: Cannot enable IRQ: {}\n", std::strerror(errno));
            return false;
        }

        return true;
    }

    template<class Rep, class Period>
    int wait_for_irq(std::chrono::duration<Rep, Period> timeout) {
        using namespace std::chrono;
        // Map "infinite" to -1
        const auto ms = duration_cast<milliseconds>(timeout);
        int tmo;

        if (ms == milliseconds::max()) {
            tmo = -1;
        } else {
            const auto clamped = ms.count() > static_cast<long long>(INT_MAX)
                               ? static_cast<long long>(INT_MAX)
                               : (ms.count() < 0 ? 0LL : ms.count());
            tmo = static_cast<int>(clamped);
        }

        return wait_for_irq_impl(tmo);
    }

  private:
    static constexpr uintptr_t mem_base = mem::get_base_addr(uio_mem);
    int fd = -1;

    void close_fd() noexcept {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }

    int wait_for_irq_impl(int timeout_ms) {
        if (fd < 0) {
            koheron::print_fmt<ERROR>("Uio: wait_for_irq on invalid fd\n");
            return -1;
        }

        struct pollfd pfd{fd, POLLIN, 0};

        for (;;) {
            int pr = ::poll(&pfd, 1, timeout_ms);
            if (pr < 0 && errno == EINTR) {
                continue; // retry on signal
            }

            if (pr == 0) {
                // timeout
                return 0;
            }

            if (pr < 0) {
                koheron::print_fmt<ERROR>("Uio: poll error: {}\n", std::strerror(errno));
                return -1;
            }

            // Check for error/hangup conditions
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                koheron::print_fmt<ERROR>("Uio: poll revents=0x{:x}\n", pfd.revents);
                return -1;
            }

            if (!(pfd.revents & POLLIN)) {
                // spurious? loop again
                continue;
            }

            // Consume the interrupt counter (auto-masks IRQ)
            uint32_t irqcnt = 0;
            ssize_t n;
            do {
                n = ::read(fd, &irqcnt, sizeof(irqcnt));
            } while (n < 0 && errno == EINTR);

            if (n != static_cast<ssize_t>(sizeof(irqcnt))) {
                koheron::print_fmt<ERROR>("Uio: read error: {}\n", std::strerror(errno));
                return -1;
            }

            // Success: return the counter (>=1). Caller should arm_irq() when ready.
            return static_cast<int>(irqcnt);
        }
    }
};

#endif // __SERVER_DRIVERS_UIO_HPP__
