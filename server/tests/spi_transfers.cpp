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
unsigned ioctls = 0;
int open_error = 0, ioctl_error = EINVAL, message_result = -1;
int last_fd = -1;
spi_ioc_transfer captured{};
bool mock_io = false;
struct IoStep { ssize_t count; int error; };
std::vector<IoStep> read_steps, write_steps;
size_t read_calls = 0, write_calls = 0;
std::vector<uint8_t> captured_write;
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
}
extern "C" int __real_open(const char*, int, ...);
extern "C" int __wrap_open(const char* path, int flags, ...) {
    check(std::strcmp(path, "/dev/spidev-test") == 0, "device path");
    check(flags & O_CLOEXEC, "descriptor inheritance");
    ++opens;
    if (open_error) { errno = open_error; return -1; }
    last_fd = __real_open("/dev/null", flags);
    return last_fd;
}
extern "C" int __wrap_open64(const char* path, int flags, ...) {
    return __wrap_open(path, flags);
}
extern "C" int __real_close(int);
extern "C" int __wrap_close(int fd) {
    const auto result = __real_close(fd);
    // A cleanup call must not replace the error from the failed configuration.
    errno = EBUSY;
    return result;
}
extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __wrap_read(int fd, void* buffer, size_t count) {
    if (!mock_io || fd != last_fd) { return __real_read(fd, buffer, count); }
    check(fcntl(fd, F_GETFD) >= 0, "read used closed descriptor");
    check(read_calls < read_steps.size(), "SPI read replayed a short transaction");
    const auto step = read_steps[read_calls++];
    if (step.count < 0) { errno = step.error; return -1; }
    check(static_cast<size_t>(step.count) <= count, "read fixture overflow");
    auto* bytes = static_cast<uint8_t*>(buffer);
    for (ssize_t i = 0; i < step.count; ++i) { bytes[i] = static_cast<uint8_t>(0xA0 + i); }
    return step.count;
}
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" ssize_t __wrap_write(int fd, const void* buffer, size_t count) {
    if (!mock_io || fd != last_fd) { return __real_write(fd, buffer, count); }
    check(fcntl(fd, F_GETFD) >= 0, "write used closed descriptor");
    check(write_calls < write_steps.size(), "SPI write replayed a short transaction");
    const auto* bytes = static_cast<const uint8_t*>(buffer);
    captured_write.assign(bytes, bytes + count);
    const auto step = write_steps[write_calls++];
    if (step.count < 0) { errno = step.error; return -1; }
    check(static_cast<size_t>(step.count) <= count, "write fixture overflow");
    return step.count;
}
extern "C" int __wrap_ioctl(int fd, unsigned long request, ...) {
    check(fd == last_fd && fcntl(fd, F_GETFD) >= 0, "invalid descriptor");
    ++ioctls;
    va_list args;
    va_start(args, request);
    void* value = va_arg(args, void*);
    va_end(args);
    if (request == SPI_IOC_MESSAGE(1)) {
        ++messages;
        captured = *static_cast<spi_ioc_transfer*>(value);
    }
    if (request == fail_request) { errno = ioctl_error; return -1; }
    if (request == SPI_IOC_MESSAGE(1) && message_result >= 0) { return message_result; }
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
    if (test == "checked-init") {
        open_error = EACCES;
        const auto denied = spi.init_checked(SPI_MODE_0, 1000000, 8);
        check(!denied && denied.error() == std::errc::permission_denied,
              "open error lost");
        check(!spi.is_ok() && ioctls == 0, "failed open configured a device");
        open_error = 0;
        unsigned expected_ioctls = 0;
        for (const auto request : {SPI_IOC_WR_MODE, SPI_IOC_WR_MAX_SPEED_HZ,
                                   SPI_IOC_WR_BITS_PER_WORD}) {
            fail_request = request;
            ioctl_error = ENOTTY;
            const auto before = ioctls;
            const auto result = spi.init_checked(SPI_MODE_0, 1000000, 8);
            check(!result && result.error() == std::errc::inappropriate_io_control_operation,
                  "cleanup replaced initialization error");
            check(errno == EBUSY, "cleanup fixture did not overwrite errno");
            check(ioctls == before + ++expected_ioctls, "configuration continued after failure");
            check(!spi.is_ok() && fcntl(last_fd, F_GETFD) == -1 && errno == EBADF,
                  "failed checked init leaked fd");
            fail_request = 0;
            check(spi.init_checked(SPI_MODE_0, 1000000, 8).has_value(), "checked retry failed");
        }
        fail_request = SPI_IOC_WR_MODE;
        check(spi.init(SPI_MODE_0, 1000000, 8) == -1 && errno == ENOTTY,
              "legacy init did not restore captured errno");
        fail_request = 0;
        check(spi.init(SPI_MODE_0, 1000000, 8) == 0, "legacy init retry failed");
        hw::SpiDev moved{std::move(spi)};
        check(moved.is_ok() && !spi.is_ok(), "move ownership");
        return 0;
    }
    if (test == "checked-invalid-device") {
        const std::array<uint8_t, 4> tx{};
        std::array<uint8_t, 4> rx{};
        for (const auto& result : {spi.set_mode_checked(SPI_MODE_0),
                                  spi.set_full_mode_checked(SPI_MODE_0),
                                  spi.set_speed_checked(1000000),
                                  spi.set_word_length_checked(8),
                                  spi.transfer_checked(tx, rx)}) {
            check(!result && result.error() == std::errc::bad_file_descriptor,
                  "invalid descriptor error lost");
        }
        check(opens == 0 && ioctls == 0, "invalid device reached kernel");
        for (const auto& result : {spi.recv_checked(rx), spi.write_checked(tx)}) {
            check(!result && result.error() == std::errc::bad_file_descriptor,
                  "byte I/O invalid descriptor error lost");
        }
        return 0;
    }
    check(spi.init(SPI_MODE_0, 1000000, 8) == 0, "init");
    const std::array<uint8_t, 4> tx{1, 2, 3, 4};
    std::array<uint8_t, 4> rx{};
    if (test == "checked-byte-counts") {
        mock_io = true;
        read_steps = {{4, 0}, {4, 0}, {4, 0}, {4, 0}, {4, 0}};
        write_steps = {{4, 0}, {4, 0}, {4, 0}};
        std::array<uint16_t, 2> words{};
        std::vector<uint16_t> vector(2);
        check(spi.recv_checked(rx) == 4, "checked array read count");
        check((rx == std::array<uint8_t, 4>{0xA0, 0xA1, 0xA2, 0xA3}), "checked array read bytes");
        check(spi.recv_checked(std::span{rx}) == 4, "checked span read count");
        check(spi.recv_checked(rx.data(), rx.size()) == 4, "checked pointer read count");
        check(spi.recv_checked(words) == 4, "checked typed array byte count");
        check(spi.recv_checked(vector) == 4, "checked typed vector byte count");
        check(std::memcmp(words.data(), rx.data(), rx.size()) == 0
              && std::memcmp(vector.data(), rx.data(), rx.size()) == 0, "typed read changed native bytes");
        check(spi.write_checked(tx) == 4 && captured_write == std::vector<uint8_t>(tx.begin(), tx.end()),
              "checked span write bytes");
        check(spi.write_checked(words.data(), words.size()) == 4
              && std::memcmp(captured_write.data(), words.data(), captured_write.size()) == 0,
              "checked typed write bytes");
        check(spi.write(words.data(), words.size()) == 4, "legacy typed write count");
        const auto before_reads = read_calls, before_writes = write_calls;
        check(spi.recv_checked(nullptr, 0) == 0 && spi.recv_checked(std::span{rx}.first(0)) == 0,
              "checked empty receive");
        check(spi.write_checked(static_cast<const uint8_t*>(nullptr), 0) == 0
              && spi.write_checked(std::span{tx}.first(0)) == 0, "checked empty write");
        std::array<uint32_t, 0> empty_array{};
        std::vector<uint32_t> empty_vector;
        check(spi.recv_checked(empty_array) == 0 && spi.recv_checked(empty_vector) == 0,
              "checked empty typed receives");
        check(spi.recv(empty_array) == 0 && spi.recv(empty_vector) == 0
              && spi.write(tx.data(), 0) == 0, "legacy empty byte I/O");
        check(read_calls == before_reads && write_calls == before_writes,
              "empty byte I/O reached kernel");
    } else if (test == "checked-byte-short") {
        mock_io = true;
        read_steps = {{2, 0}, {0, 0}, {2, 0}, {0, 0}};
        write_steps = {{2, 0}, {0, 0}, {2, 0}, {0, 0}};
        rx.fill(0xFF);
        check(spi.recv_checked(rx) == 2 && read_calls == 1, "checked short read count or replay");
        check(rx[0] == 0xA0 && rx[1] == 0xA1 && rx[2] == 0xFF && rx[3] == 0xFF,
              "short read touched trailing bytes");
        check(spi.recv_checked(rx) == 0 && read_calls == 2, "checked EOF read count or replay");
        check(spi.recv(rx) == 2 && read_calls == 3, "legacy short read replayed");
        check(spi.recv(rx) == 0 && read_calls == 4, "legacy EOF read replayed");
        check(spi.write_checked(tx) == 2 && write_calls == 1, "checked short write count or replay");
        check(spi.write_checked(tx) == 0 && write_calls == 2, "checked zero write count or replay");
        check(spi.write(tx.data(), tx.size()) == -1 && errno == EIO && write_calls == 3,
              "legacy short write succeeded or replayed");
        check(spi.write(tx.data(), tx.size()) == -1 && errno == EIO && write_calls == 4,
              "legacy zero write succeeded or replayed");
    } else if (test == "checked-byte-errors") {
        mock_io = true;
        read_steps = {{-1, EREMOTEIO}, {-1, ETIMEDOUT}, {-1, EINTR}, {4, 0}};
        write_steps = {{-1, ENOSPC}, {-1, EREMOTEIO}, {-1, EINTR}, {4, 0}};
        const auto read_error = spi.recv_checked(rx);
        const auto write_error = spi.write_checked(tx);
        errno = 0;
        check(!read_error && read_error.error() == std::error_code(EREMOTEIO, std::generic_category()),
              "checked read error depended on ambient errno");
        check(!write_error && write_error.error() == std::errc::no_space_on_device,
              "checked write error depended on ambient errno");
        check(read_calls == 1 && write_calls == 1, "failed byte I/O was retried");
        check(spi.recv(rx) == -1 && errno == ETIMEDOUT && read_calls == 2,
              "legacy read error not retained");
        check(spi.write(tx.data(), tx.size()) == -1 && errno == EREMOTEIO && write_calls == 2,
              "legacy write error not retained");
        check(spi.recv_checked(rx) == 4 && read_calls == 4, "interrupted read did not retry");
        check(spi.write_checked(tx) == 4 && write_calls == 4, "interrupted write did not retry");
    } else if (test == "checked-byte-bounds") {
        mock_io = true;
        const auto null_read = spi.recv_checked(nullptr, 1);
        const auto null_write = spi.write_checked(static_cast<const uint8_t*>(nullptr), 1);
        check(!null_read && null_read.error() == std::errc::invalid_argument, "null read buffer error");
        check(!null_write && null_write.error() == std::errc::invalid_argument, "null write buffer error");
        const auto oversized_read = spi.recv_checked(rx.data(), static_cast<size_t>(INT_MAX) + 1);
        uint32_t word{};
        const auto oversized_write = spi.write_checked(&word, UINT32_MAX / sizeof(word) + 1);
        check(!oversized_read && oversized_read.error() == std::errc::value_too_large,
              "oversized read error");
        check(!oversized_write && oversized_write.error() == std::errc::value_too_large,
              "typed write overflow error");
        check(read_calls == 0 && write_calls == 0, "invalid byte I/O reached kernel");
    } else if (test == "checked-transfer") {
        check(spi.transfer_checked(tx, rx).has_value(), "checked full duplex");
        check(spi.transfer(tx, rx) == 0 && captured.len == tx.size(), "legacy transfer result");
        check(spi.transfer_checked(tx, rx, 2).has_value() && captured.len == 2, "checked prefix");
        check(spi.transfer_checked(tx, 2).has_value() && captured.rx_buf == 0, "checked TX only");
        check(spi.transfer_checked(rx, 2).has_value() && captured.tx_buf == 0, "checked RX only");
        check(spi.transfer_checked(rx.data(), nullptr, rx.size()).has_value(), "checked raw TX");
        check(spi.transfer_checked(nullptr, rx.data(), rx.size()).has_value(), "checked raw RX");
        const auto before = messages;
        const auto short_rx = std::span{rx}.first(2);
        const auto empty_tx = std::span{tx}.first(0);
        const auto empty_rx = std::span{rx}.first(0);
        for (const auto& result : {spi.transfer_checked(tx, rx, 5), spi.transfer_checked(tx, 5),
                                  spi.transfer_checked(rx, 5), spi.transfer_checked(tx, short_rx),
                                  spi.transfer_checked(nullptr, nullptr, 1)}) {
            check(!result && result.error() == std::errc::invalid_argument, "invalid argument error");
        }
        check(spi.transfer_checked(empty_tx, empty_rx).has_value(), "checked empty spans");
        check(spi.transfer_checked(nullptr, nullptr, 0).has_value(), "checked empty raw transfer");
        check(spi.transfer_checked(tx, rx, 0).has_value(), "checked empty prefix");
        const auto oversized = spi.transfer_checked(rx.data(), nullptr, static_cast<size_t>(INT_MAX) + 1);
        check(!oversized && oversized.error() == std::errc::value_too_large, "length error lost");
        check(messages == before, "invalid or empty transfer reached kernel");
        fail_request = SPI_IOC_MESSAGE(1);
        ioctl_error = EREMOTEIO;
        const auto failed = spi.transfer_checked(tx, rx);
        errno = 0;
        check(!failed && failed.error() == std::error_code(EREMOTEIO, std::generic_category()),
              "transfer depended on ambient errno");
        check(messages == before + 1, "failed transaction retried");
        check(spi.transfer(tx, rx) == -1 && errno == EREMOTEIO, "legacy transfer error lost");
        fail_request = 0;
        for (const auto count : {0, 2}) {
            message_result = count;
            const auto result = spi.transfer_checked(tx, rx);
            check(!result && result.error() == std::errc::io_error, "short transfer reported success");
            check(spi.transfer(tx, rx) == -1 && errno == EIO, "legacy short transfer reported success");
        }
    } else if (test == "checked-settings") {
        ioctl_error = ENOTTY;
        fail_request = SPI_IOC_WR_MODE;
        const auto mode = spi.set_mode_checked(SPI_MODE_3);
        fail_request = SPI_IOC_WR_MODE32;
        const auto full_mode = spi.set_full_mode_checked(SPI_MODE_3);
        fail_request = SPI_IOC_WR_MAX_SPEED_HZ;
        const auto speed = spi.set_speed_checked(4000000);
        fail_request = SPI_IOC_WR_BITS_PER_WORD;
        const auto word = spi.set_word_length_checked(16);
        for (const auto& result : {mode, full_mode, speed, word}) {
            check(!result && result.error() == std::errc::inappropriate_io_control_operation,
                  "setting error lost");
        }
        check(spi.set_word_length(16) == -1 && errno == ENOTTY, "legacy setting error lost");
        check(spi.transfer_checked(tx, rx).has_value() && captured.speed_hz == 1000000
              && captured.bits_per_word == 8, "failed checked setting affected transfer");
        fail_request = 0;
        check(spi.set_mode_checked(SPI_MODE_3).has_value()
              && spi.set_full_mode_checked(SPI_MODE_3).has_value()
              && spi.set_speed_checked(2000000).has_value()
              && spi.set_word_length_checked(16).has_value(), "checked settings failed");
        check(spi.transfer_checked(tx, rx).has_value() && captured.speed_hz == 2000000
              && captured.bits_per_word == 16, "checked settings not used");
    } else { check(false, "unknown case"); }
}
