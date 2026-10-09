#include <sys/mman.h>
#include <sys/eventfd.h>
#include "server/drivers/uio.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <termios.h>
using namespace std::chrono_literals;

namespace {
bool fail_eventfd = false;
std::atomic<int> wakeup_fd{-1};
std::atomic<bool> hold_wakeup{false}, wakeup_entered{false}, poll_entered{false};
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
template<class F> void wait_until(F condition, const char* message = "condition timed out") {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!condition()) {
        check(std::chrono::steady_clock::now() < deadline, message);
        std::this_thread::sleep_for(1ms);
    }
}
size_t fd_count() {
    size_t count = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator("/proc/self/fd")) { ++count; }
    return count;
}
struct Device {
    int master;
    std::filesystem::path root;
    explicit Device(const char* directory) : master(posix_openpt(O_RDWR | O_NOCTTY)), root(directory) {
        check(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0, "PTY creation");
        const char* name = ptsname(master);
        const int slave = ::open(name, O_RDWR | O_NOCTTY);
        termios term{};
        check(slave >= 0 && tcgetattr(slave, &term) == 0, "PTY attributes");
        cfmakeraw(&term);
        check(tcsetattr(slave, TCSANOW, &term) == 0, "raw PTY");
        ::close(slave);
        std::filesystem::remove(root / "dev/uio0");
        std::filesystem::create_symlink(name, root / "dev/uio0");
    }
    ~Device() { ::close(master); }
    void arm_word() {
        pollfd pfd{master, POLLIN, 0};
        check(::poll(&pfd, 1, 1000) == 1, "missing arm word");
        uint32_t word{};
        check(::read(master, &word, sizeof(word)) == sizeof(word) && word == 1, "arm word");
    }
    void interrupt(uint32_t counter) {
        check(::write(master, &counter, sizeof(counter)) == sizeof(counter), "write interrupt");
    }
    void backing_file() {
        const auto path = root / "dev/backing";
        const int file = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
        check(file >= 0 && ftruncate(file, 4096) == 0, "backing file");
        ::close(file);
        std::filesystem::remove(root / "dev/uio0");
        std::filesystem::create_symlink(path, root / "dev/uio0");
    }
};
bool mapped(void* p) {
    unsigned char residency{};
    return ::mincore(p, 4096, &residency) == 0;
}
}
extern "C" int __real_eventfd(unsigned int, int);
extern "C" int __wrap_eventfd(unsigned int value, int flags) {
    if (fail_eventfd) { errno = EMFILE; return -1; }
    check((flags & (EFD_NONBLOCK | EFD_CLOEXEC)) == (EFD_NONBLOCK | EFD_CLOEXEC), "eventfd flags");
    const int fd = __real_eventfd(value, flags);
    wakeup_fd = fd;
    return fd;
}
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t size) {
    if (fd == wakeup_fd && hold_wakeup) {
        wakeup_entered = true;
        hold_wakeup.wait(true);
    }
    return __real_write(fd, data, size);
}
extern "C" int __real_poll(pollfd*, nfds_t, int);
extern "C" int __wrap_poll(pollfd* fds, nfds_t count, int timeout) {
    if (count == 2 && fds[1].fd == wakeup_fd) { poll_entered = true; }
    return __real_poll(fds, count, timeout);
}
// Fortified builds may select this entry point instead of poll.
extern "C" int __wrap___poll_chk(pollfd* fds, nfds_t count, int timeout, size_t bytes) {
    check(count <= bytes / sizeof(pollfd), "poll buffer bounds");
    return __wrap_poll(fds, count, timeout);
}
int main(int argc, char** argv) {
    check(argc == 3, "fixture root and case");
    Device device{argv[1]};
    const std::string_view test{argv[2]};
    const auto initial_fds = fd_count();
    std::atomic<int> calls{0};
    if (test == "default") {
        Uio<0> uio;
        check(uio.open() >= 0, "open");
        check(uio.listen([&](int rc) { check(rc == 42, "default IRQ result"); ++calls; }), "default listen");
        device.arm_word();
        std::this_thread::sleep_for(20ms);
        check(uio.is_running() && calls == 0, "default timeout expired immediately");
        device.interrupt(42);
        wait_until([&] { return calls == 1; });
        uio.unlisten();
        check(calls == 1, "cancellation delivered an extra callback");
    } else if (test == "finite" || test == "infinite") {
        Uio<0> uio;
        check(uio.open() >= 0, "open");
        const auto timeout = test == "finite" ? 5s : std::chrono::milliseconds::max();
        check(uio.listen([&](int) { ++calls; }, timeout), "listen");
        device.arm_word();
        std::this_thread::sleep_for(20ms);
        const auto start = std::chrono::steady_clock::now();
        std::thread cancel([&] { uio.cancel(); });
        cancel.join();
        uio.unlisten();
        check(std::chrono::steady_clock::now() - start < 500ms, "cancellation waited for timeout/IRQ");
        check(calls == 0, "cancel was reported as timeout/error");
    } else if (test == "restart") {
        Uio<0> uio;
        check(uio.open() >= 0, "open");
        for (int i = 0; i < 30; ++i) {
            check(uio.listen([&](int) { ++calls; }, 5s), "restart listen");
            device.arm_word();
            uio.cancel();
            // Restart joins the canceled worker itself, without reviving it.
        }
        check(uio.listen([&](int rc) { check(rc == 7, "stale cancellation"); ++calls; uio.unlisten(); }, 500ms),
              "final restart");
        device.arm_word();
        device.interrupt(7);
        wait_until([&] { return !uio.is_running(); });
        uio.unlisten();
        check(calls == 1, "restart callback count");
    } else if (test == "concurrent_restart") {
        Uio<0> uio;
        check(uio.open() >= 0 && uio.listen([&](int) { ++calls; }), "initial listener");
        device.arm_word();
        wait_until([&] { return poll_entered.load(); }, "worker did not enter poll");
        hold_wakeup = true;
        std::thread cancel([&] { uio.cancel(); });
        wait_until([&] { return wakeup_entered.load(); }, "cancel did not enter wakeup write");
        // Let the old worker finish before cancel's delayed eventfd write.
        device.interrupt(17);
        std::atomic<bool> restarting{false}, restarted{false};
        std::thread restart([&] {
            restarting = true;
            check(uio.listen([&](int rc) { check(rc == 99, "old wakeup canceled new worker"); ++calls; uio.cancel(); }),
                  "concurrent restart");
            restarted = true;
        });
        wait_until([&] { return restarting.load(); }, "restart thread did not start");
        std::this_thread::sleep_for(20ms);
        check(!restarted && calls == 0, "restart bypassed in-flight cancellation");
        hold_wakeup = false;
        hold_wakeup.notify_all();
        cancel.join();
        restart.join();
        device.arm_word();
        device.interrupt(99);
        wait_until([&] { return !uio.is_running(); });
        uio.unlisten();
        check(calls == 1, "concurrent restart callback count");
    } else if (test == "sync") {
        Uio<0> uio;
        check(uio.open() >= 0, "open");
        check(fd_count() == initial_fds + 1, "synchronous open allocated a wakeup fd");
        check(uio.wait_for_irq(0ms) == 0, "nonblocking poll");
        std::thread interrupt([&] { std::this_thread::sleep_for(20ms); device.interrupt(9); });
        const int result = uio.wait_for_irq(-1ms);
        interrupt.join();
        check(result == 9, "negative timeout did not wait for IRQ");
        check(uio.listen([&](int) { ++calls; }, 5s), "listen before sync wait");
        device.arm_word();
        uio.unlisten();
        device.interrupt(10);
        check(uio.wait_for_irq(500ms) == 10 && calls == 0, "sync wait consumed cancellation");
    } else if (test == "move_active") {
        Uio<0> source;
        const int fd = source.open();
        check(fd >= 0 && source.listen([&](int) { ++calls; }, 5s), "source listener");
        device.arm_word();
        std::this_thread::sleep_for(20ms);
        const auto start = std::chrono::steady_clock::now();
        Uio<0> target{std::move(source)};
        check(std::chrono::steady_clock::now() - start < 500ms, "move blocked on listener");
        check(source.fd() == -1 && !source.is_running() && target.fd() == fd && !target.is_running(),
              "move left active source");
        check(target.listen([&](int rc) { check(rc == 11, "moved listener result"); ++calls; target.cancel(); }),
              "moved listener restart");
        device.arm_word();
        device.interrupt(11);
        wait_until([&] { return !target.is_running(); });
        target.unlisten();
        check(calls == 1, "move invoked canceled callback");
    } else if (test == "move_map") {
        device.backing_file();
        void* pointer;
        std::unique_ptr<Uio<0>> target;
        {
            Uio<0> source;
            check(source.open() >= 0, "mapped source open");
            pointer = source.mmap();
            check(pointer != nullptr, "mapped source");
            *static_cast<uint32_t*>(pointer) = 0x12345678;
            target = std::make_unique<Uio<0>>(std::move(source));
        }
        check(mapped(pointer), "source destructor unmapped moved memory");
        check(*static_cast<uint32_t*>(pointer) == 0x12345678, "mapping data");
        Uio<0> destination;
        check(destination.open() >= 0, "mapped destination open");
        void* discarded = destination.mmap();
        check(discarded != nullptr && discarded != pointer, "mapped destination");
        const int old_fd = destination.fd();
        destination = std::move(*target);
        check(!mapped(discarded) && mapped(pointer), "move assignment mapping ownership");
        check(fcntl(old_fd, F_GETFD) < 0 && errno == EBADF, "move assignment leaked destination fd");
        target.reset();
        check(mapped(pointer), "moved-from destruction unmapped source");
        destination.unmap();
        check(!mapped(pointer), "mapping was not released by new owner");
    } else if (test == "reopen") {
        Uio<0> uio;
        const int old_fd = uio.open();
        check(old_fd >= 0 && uio.listen([&](int) { ++calls; }, 5s), "reopen listener");
        device.arm_word();
        std::this_thread::sleep_for(20ms);
        check(uio.open() >= 0 && !uio.is_running() && calls == 0, "reopen left old worker active");
        check(fcntl(old_fd, F_GETFD) < 0 && errno == EBADF, "reopen leaked old fd");
    } else if (test == "eventfd_failure") {
        Uio<0> uio;
        check(uio.open() >= 0, "open");
        fail_eventfd = true;
        check(!uio.listen([&](int) { ++calls; }) && !uio.is_running(), "eventfd failure ignored");
        fail_eventfd = false;
        check(uio.listen([&](int) { ++calls; }, 5s), "eventfd failure not retryable");
        device.arm_word();
        uio.unlisten();
        check(calls == 0, "failed/retried listener invoked callback");
    } else { check(false, "unknown case"); }
    check(fd_count() == initial_fds, "UIO or wakeup descriptor leaked");
}
