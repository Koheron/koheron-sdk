#ifndef __SERVER_DRIVERS_UIO_HPP__
#define __SERVER_DRIVERS_UIO_HPP__

#include "server/runtime/syslog.hpp"
#include "server/context/memory_catalog.hpp"

#include <atomic>
#include <thread>
#include <functional>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <cstdint>
#include <climits>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <utility>
#include <string_view>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

template <MemID uio_mem>
class Uio
{
  public:
    Uio() = default;

    // Non-copyable, movable (unique FD owner)
    Uio(const Uio&) = delete;
    Uio& operator=(const Uio&) = delete;
    Uio(Uio&& other) noexcept : fd_(std::exchange(other.fd(), -1)) {}

    Uio& operator=(Uio&& other) noexcept {
        if (this != &other) {
            close_fd();
            fd = std::exchange(other.fd, -1);
        }

        return *this;
    }

    ~Uio() {
        unlisten();
        unmap();
        close_fd();
    }

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

            if (addr == phys_addr) {
                const auto dev_path = fs::path("/dev") / name;
                new_fd = ::open(dev_path.c_str(), O_RDWR | O_CLOEXEC);

                if (new_fd < 0) {
                    koheron::print_fmt<ERROR>("Uio: Cannot open {}: {}\n",
                                              dev_path, std::strerror(errno));
                } else {
                    koheron::print_fmt("Uio: {} @ {} ready\n", mem_name, dev_path);
                }

                break;
            }
        }

        if (new_fd < 0) {
            koheron::print_fmt<ERROR>("Uio: No device with base {:#x}\n", phys_addr);
            return -1;
        }

        close_fd();
        fd_ = new_fd;
        return fd_;
    }

    int fd() const noexcept { return fd_; }

    // ------------------------------------------------------------------------
    // Interrupt API
    // ------------------------------------------------------------------------

    bool arm_irq() {
        if (fd_ < 0) {
            koheron::print_fmt<ERROR>("Uio: arm_irq on invalid fd\n");
            return false;
        }

        uint32_t arm = 1;
        ssize_t n;
        do {
            n = ::write(fd_, &arm, sizeof(arm));
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

    template <class Fn>
    bool listen(Fn&& fn,
                std::chrono::milliseconds timeout = std::chrono::milliseconds{-1}) {
        if (fd_ < 0) {
            koheron::print_fmt<ERROR>("Uio: on_irq called with invalid fd\n");
            return false;
        }

        if (running_.exchange(true)) {
            koheron::print_fmt<ERROR>("Uio: on_irq already running\n");
            return false;
        }

        auto cb = std::function<void(int)>(std::forward<Fn>(fn));

        if (!cb) {
            running_ = false;
            return false;
        }

        // (Re)arm before entering the loop so the first IRQ can arrive.
        if (!arm_irq()) {
            running_ = false;
            return false;
        }

        worker_ = std::thread([this, timeout, cb = std::move(cb)]() mutable {
            while (running_) {
                const int rc = wait_for_irq(timeout);

                if (rc <= 0) { // Error or Timeout
                    cb(rc);
                    break;
                }

                // Oe or more interrupts have occurred;
                // Re-arm *before* calling the user callback to minimize dead time.
                if (!arm_irq()) {
                    cb(-1);
                    break;
                }

                cb(rc);
            }

            running_ = false;
        });

        return true;
    }

    // Stop the async loop and join the thread.
    void unlisten() {
        if (!running_.exchange(false)) {
            return;
        }

        if (worker_.joinable()) {
            if (std::this_thread::get_id() == worker_.get_id()) {
                // don't join self; let the owner thread join later
                worker_.detach();
            } else {
                worker_.join();
            }
        }
    }

    void cancel() noexcept { running_.store(false); }
    bool is_running() const noexcept { return running_.load(); }

    // ------------------------------------------------------------------------
    // Memory map
    // ------------------------------------------------------------------------

    void* mmap() {
        if (fd_ < 0) {
            koheron::print_fmt<ERROR>("Uio: mmap on invalid fd\n");
            return nullptr;
        }

        // If already mapped, unmap first
        unmap();

        const size_t pg = page_size();
        int map_index = 0; // maps/map0
        // In UIO: offset selects the map index (offset = page_size * map_index).
        const off_t off = static_cast<off_t>(map_index) * static_cast<off_t>(pg);
        const size_t len = align_up(size, pg);  // Length: align to page size to be safe

        void* p = ::mmap(nullptr, len, protection, MAP_SHARED, fd_, off);

        if (p == MAP_FAILED) {
            koheron::print_fmt<ERROR>(
                "Uio: mmap({}, len={:#x}) failed: {}\n",
                mem_name, len, std::strerror(errno));
            return nullptr;
        } else {
            koheron::print_fmt("Uio: {} memory mapped\n", mem_name);
        }

        map_base_ = p;
        map_len_ = len;
        return map_base_;
    }

    void unmap() noexcept {
        if (map_base_ && map_base_ != MAP_FAILED) {
            ::munmap(map_base_, map_len_);
        }

        map_base_  = MAP_FAILED;
        map_len_   = 0;
    }

  private:
    static constexpr uintptr_t phys_addr = mem::get_base_addr(uio_mem);
    static constexpr uint32_t size = mem::get_total_size(uio_mem);
    static constexpr int protection = mem::get_protection(uio_mem);
    static constexpr std::string_view mem_name = mem::get_name(uio_mem);

    int fd_ = -1;

    // Worker thread for asynchrous API
    std::thread worker_;
    std::atomic<bool> running_{false};

    // Memory map
    void* map_base_ = MAP_FAILED;
    size_t map_len_ = 0;

    void close_fd() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    int wait_for_irq_impl(int timeout_ms) {
        if (fd_ < 0) {
            koheron::print_fmt<ERROR>("Uio: wait_for_irq on invalid fd\n");
            return -1;
        }

        struct pollfd pfd{fd_, POLLIN, 0};

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
                n = ::read(fd_, &irqcnt, sizeof(irqcnt));
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
