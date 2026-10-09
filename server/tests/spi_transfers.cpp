#include "server/hardware/spi_manager.hpp"
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

namespace {
unsigned opens = 0, messages = 0;
unsigned long fail_request = 0;
int last_fd = -1;
spi_ioc_transfer captured{};
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
}
extern "C" int __real_open(const char*, int, ...);
extern "C" int __wrap_open(const char* path, int flags, ...) {
    check(std::strcmp(path, "/dev/spidev-test") == 0, "device path");
    check(flags & O_CLOEXEC, "descriptor inheritance");
    ++opens;
    last_fd = __real_open("/dev/null", flags);
    return last_fd;
}
extern "C" int __wrap_open64(const char* path, int flags, ...) {
    return __wrap_open(path, flags);
}
extern "C" int __wrap_ioctl(int fd, unsigned long request, ...) {
    check(fd == last_fd && fcntl(fd, F_GETFD) >= 0, "invalid descriptor");
    va_list args;
    va_start(args, request);
    void* value = va_arg(args, void*);
    va_end(args);
    if (request == SPI_IOC_MESSAGE(1)) {
        ++messages;
        captured = *static_cast<spi_ioc_transfer*>(value);
    }
    if (request == fail_request) { errno = EINVAL; return -1; }
    return request == SPI_IOC_MESSAGE(1) ? static_cast<int>(captured.len) : 0;
}
// 32-bit glibc redirects ioctl when the toolchain enables the time64 ABI.
extern "C" int __wrap___ioctl_time64(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    void* value = va_arg(args, void*);
    va_end(args);
    return __wrap_ioctl(fd, request, value);
}
int main(int argc, char** argv) {
    check(argc == 2, "case");
    const std::string_view test{argv[1]};
    hw::SpiDev spi{"spidev-test"};
    if (test == "init") {
        for (const auto request : {SPI_IOC_WR_MODE, SPI_IOC_WR_MAX_SPEED_HZ,
                                   SPI_IOC_WR_BITS_PER_WORD}) {
            fail_request = request;
            check(spi.init(SPI_MODE_0, 1000000, 8) < 0, "failed init reported success");
            check(!spi.is_ok(), "failed initialization left device ready");
            check(fcntl(last_fd, F_GETFD) == -1 && errno == EBADF, "failed init leaked fd");
            fail_request = 0;
            const auto before = opens;
            check(spi.init(SPI_MODE_0, 1000000, 8) == 0 && spi.is_ok(), "retry failed");
            check(opens == before + 1, "retry did not reopen");
        }
        return 0;
    }
    check(spi.init(SPI_MODE_0, 1000000, 8) == 0, "init");
    const std::array<uint8_t, 4> tx{1, 2, 3, 4};
    std::array<uint8_t, 4> rx{};
    if (test == "empty") {
        check(spi.transfer(std::span{tx}.first(0), rx) == 0, "receive only");
        // Like the kernel's u64_to_user_ptr, truncate to the native pointer width.
        check(captured.tx_buf == 0 && static_cast<uintptr_t>(captured.rx_buf) == reinterpret_cast<uintptr_t>(rx.data())
              && captured.len == rx.size(), "empty TX exposed a non-null kernel buffer");
        check(spi.transfer(tx, std::span{rx}.first(0)) == 0, "transmit only");
        check(captured.rx_buf == 0 && static_cast<uintptr_t>(captured.tx_buf) == reinterpret_cast<uintptr_t>(tx.data()),
              "empty RX exposed a non-null kernel buffer");
    } else if (test == "settings") {
        fail_request = SPI_IOC_WR_MAX_SPEED_HZ;
        check(spi.set_speed(4000000) < 0, "speed failure");
        check(spi.transfer(tx, rx) == 0 && captured.speed_hz == 1000000,
              "failed speed setting affected transfer");
        fail_request = SPI_IOC_WR_BITS_PER_WORD;
        check(spi.set_word_length(16) < 0, "word length failure");
        check(spi.transfer(tx, rx) == 0 && captured.bits_per_word == 8,
              "failed word length setting affected transfer");
        fail_request = 0;
        check(spi.set_speed(2000000) == 0 && spi.set_word_length(16) == 0, "accepted settings");
        check(spi.transfer(tx, rx) == 0 && captured.speed_hz == 2000000
              && captured.bits_per_word == 16, "accepted settings not used");
    } else if (test == "bounds") {
        check(spi.transfer(tx, rx, 5) < 0, "oversized full-duplex count accepted");
        check(spi.transfer(tx, 5) < 0, "oversized TX count accepted");
        check(spi.transfer(rx, 5) < 0, "oversized RX count accepted");
        const std::array<uint8_t, 2> small_tx{};
        check(spi.transfer(small_tx, rx, 3) < 0, "short TX ignored");
        std::array<uint8_t, 2> small_rx{};
        check(spi.transfer(tx, small_rx, 3) < 0, "short RX ignored");
        check(messages == 0, "invalid bounds reached kernel");
    } else if (test == "length") {
        uint8_t byte{};
        const auto too_large = static_cast<size_t>(INT_MAX) + 1;
        check(spi.transfer(&byte, nullptr, too_large) < 0, "oversized transfer accepted");
        check(spi.recv(&byte, too_large) < 0, "oversized receive accepted");
        uint32_t word{};
        check(spi.write(&word, UINT32_MAX / sizeof(word) + 1) < 0,
              "write length overflow accepted");
        check(messages == 0, "oversized transfer reached kernel");
        check(spi.transfer(nullptr, nullptr, 1) < 0, "missing buffers accepted");
        check(spi.recv(nullptr, 1) < 0, "missing receive buffer accepted");
        check(spi.write(static_cast<const uint8_t*>(nullptr), 1) < 0,
              "missing write buffer accepted");
    } else if (test == "normal") {
        check(spi.transfer(tx, rx) == 0 && messages == 1, "full duplex");
        check(static_cast<uintptr_t>(captured.tx_buf) == reinterpret_cast<uintptr_t>(tx.data())
              && static_cast<uintptr_t>(captured.rx_buf) == reinterpret_cast<uintptr_t>(rx.data())
              && captured.len == tx.size(), "full-duplex buffers");
        check(spi.transfer(tx, rx, 2) == 0 && captured.len == 2, "prefix");
        check(spi.transfer(tx, std::span{rx}.first(2)) < 0 && messages == 2, "mismatched lengths");
        check(spi.transfer(tx, rx, 0) == 0 && messages == 2, "empty transfer touched hardware");
        fail_request = SPI_IOC_MESSAGE(1);
        check(spi.transfer(tx, rx) < 0 && messages == 3, "kernel error propagation");
        check(spi.write(tx.data(), tx.size()) == 4, "ordinary write");
        check(spi.recv(rx) == 0, "EOF receive");
        hw::SpiDev moved{std::move(spi)};
        check(moved.is_ok() && !spi.is_ok(), "move ownership");
        check(spi.transfer(tx, rx) < 0 && messages == 3, "invalid device used");
    } else { check(false, "unknown case"); }
}
