#include "server/hardware/i2c_manager.hpp"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <thread>

namespace {
unsigned entries = 0, reads = 0, writes = 0, addresses = 0;
int device_fd = -1, selected = -1;
bool reject_address = false;
bool verify_address = false;
thread_local int expected_address = -1;
ssize_t read_result = -2, write_result = -2; // -2 means complete the request
std::vector<uint8_t> sent;
constexpr auto device = "i2c-transfer-test";
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
}
extern "C" DIR* __wrap_opendir(const char* path) {
    check(std::strcmp(path, "/sys/class/i2c-dev") == 0, "discovery path");
    entries = 0;
    return reinterpret_cast<DIR*>(&entries);
}
extern "C" dirent* __wrap_readdir(DIR*) {
    static dirent entry{};
    if (entries++) { return nullptr; }
    std::strcpy(entry.d_name, device);
    return &entry;
}
extern "C" dirent64* __wrap_readdir64(DIR*) {
    static dirent64 entry{};
    if (entries++) { return nullptr; }
    std::strcpy(entry.d_name, device);
    return &entry;
}
extern "C" int __wrap_closedir(DIR*) { return 0; }
extern "C" int __real_open(const char*, int, ...);
extern "C" int __wrap_open(const char* path, int flags, ...) {
    check(std::string(path) == std::string("/dev/") + device, "device path");
    device_fd = __real_open("/dev/null", flags);
    return device_fd;
}
extern "C" int __wrap_open64(const char* path, int flags, ...) {
    return __wrap_open(path, flags);
}
extern "C" int __wrap_ioctl(int fd, unsigned long request, ...) {
    check(fd == device_fd && request == I2C_SLAVE_FORCE, "address ioctl");
    va_list args;
    va_start(args, request);
    const int addr = va_arg(args, int);
    va_end(args);
    ++addresses;
    if (reject_address) { errno = EIO; return -1; }
    selected = addr;
    return 0;
}
extern "C" int __wrap___ioctl_time64(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    const int addr = va_arg(args, int);
    va_end(args);
    return __wrap_ioctl(fd, request, addr);
}
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" ssize_t __wrap_write(int fd, const void* buffer, size_t size) {
    if (fd != device_fd) { return __real_write(fd, buffer, size); }
    ++writes;
    if (verify_address) { check(selected == expected_address, "write used another thread's address"); }
    check(size <= 64, "unbounded write reached kernel");
    if (size != 0) {
        const auto* bytes = static_cast<const uint8_t*>(buffer);
        sent.assign(bytes, bytes + size);
    }
    if (write_result == -1) { errno = EINTR; }
    return write_result == -2 ? static_cast<ssize_t>(size) : write_result;
}
extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __wrap_read(int fd, void* buffer, size_t size) {
    if (fd != device_fd) { return __real_read(fd, buffer, size); }
    ++reads;
    if (verify_address) { check(selected == expected_address, "read used another thread's address"); }
    check(size <= 64, "unbounded read reached kernel");
    if (read_result == -1) { errno = EINTR; return -1; }
    const auto count = read_result == -2 ? size : std::min(size, static_cast<size_t>(read_result));
    if (count) { std::memset(buffer, 0x5a, count); }
    return static_cast<ssize_t>(count);
}

template<class T>
concept Writable = requires(hw::I2cDev& d, const T& value) { d.write(42, value); };
template<class T>
concept Readable = requires(hw::I2cDev& d, T& value) { d.read(42, value); };
static_assert(!Writable<std::array<std::string, 1>> && !Readable<std::array<std::string, 1>>);
static_assert(!Writable<std::vector<std::string>> && !Readable<std::vector<std::string>>);
static_assert(!Writable<std::string> && !Readable<std::string>);

template<class T> void check_write(hw::I2cDev& dev, const T& value, const void* bytes, size_t size) {
    const auto before = writes;
    check(dev.write(42, value) == static_cast<int>(size), "typed write result");
    check(writes == before + 1 && sent.size() == size
          && std::memcmp(sent.data(), bytes, size) == 0, "typed write bytes/count");
}
int main(int argc, char** argv) {
    check(argc == 2, "case");
    const std::string_view test{argv[1]};
    hw::I2cManager manager;
    check(manager.init() == 0, "discovery");
    auto& dev = manager.get(device);
    check(dev.is_ok(), "initialization");
    std::array<uint8_t, 4> bytes{};
    if (test == "short_read") {
        for (const ssize_t result : {0, 2, -1}) {
            reads = 0;
            read_result = result;
            check(dev.read(42, bytes) == -1, "incomplete read reported success");
            check(reads == 1, "incomplete read replayed bus transaction");
            check(errno == (result < 0 ? EINTR : EIO), "read error code");
        }
    } else if (test == "short_write") {
        for (const ssize_t result : {0, 2, -1}) {
            writes = 0;
            write_result = result;
            check(dev.write(42, bytes) == -1, "incomplete write reported success");
            check(writes == 1, "incomplete write replayed bus transaction");
            check(errno == (result < 0 ? EINTR : EIO), "write error code");
        }
    } else if (test == "bounds") {
        uint8_t byte{};
        for (const auto size : {static_cast<size_t>(INT_MAX) + 1, std::numeric_limits<size_t>::max()}) {
            check(dev.read(42, &byte, size) == -1, "oversized read accepted");
            check(dev.write(42, &byte, size) == -1, "oversized write accepted");
        }
        check(reads == 0 && writes == 0 && addresses == 0, "invalid lengths reached kernel");
        check(dev.read(42, static_cast<uint8_t*>(nullptr), 1) == -1, "null read accepted");
        check(dev.write(42, static_cast<const uint8_t*>(nullptr), 1) == -1, "null write accepted");
    } else if (test == "address") {
        for (const auto addr : {-1, 128}) {
            check(dev.read(addr, bytes) == -1 && dev.write(addr, bytes) == -1, "invalid address accepted");
        }
        check(reads == 0 && writes == 0 && addresses == 0, "invalid address reached kernel");
        check(dev.write(42, bytes) == 4 && selected == 42 && addresses == 1, "select address");
        check(dev.read(42, bytes) == 4 && addresses == 1, "cached address reselected");
        reject_address = true;
        check(dev.write(43, bytes) == -1 && writes == 1, "failed address transferred");
        reject_address = false;
        check(dev.write(43, bytes) == 4 && selected == 43 && addresses == 3, "failed address was cached");
    } else if (test == "concurrent") {
        verify_address = true;
        std::array<std::thread, 4> threads;
        for (size_t i = 0; i < threads.size(); ++i) {
            threads[i] = std::thread([&, i] {
                expected_address = 42 + static_cast<int>(i);
                std::array<uint8_t, 2> data{};
                for (int n = 0; n < 1000; ++n) {
                    check(dev.write(expected_address, data) == 2, "concurrent write");
                    check(dev.read(expected_address, data) == 2, "concurrent read");
                }
            });
        }
        for (auto& thread : threads) { thread.join(); }
        check(reads == 4000 && writes == 4000, "concurrent transfer count");
    } else if (test == "typed") {
        const uint8_t byte = 0xa5;
        const uint16_t word = 0x1234;
        const uint32_t dword = 0x12345678;
        const std::array<uint16_t, 3> array{0x1234, 0x5678, 0x9abc};
        const std::vector<uint32_t> vector{0x12345678, 0x9abcdef0};
        check_write(dev, byte, &byte, sizeof(byte));
        check_write(dev, word, &word, sizeof(word));
        check_write(dev, dword, &dword, sizeof(dword));
        check_write(dev, array, array.data(), sizeof(array));
        check_write(dev, vector, vector.data(), vector.size() * sizeof(uint32_t));
        uint16_t value{};
        check(dev.read(42, value) == 2 && value == 0x5a5a, "scalar read");
        std::array<uint16_t, 2> out{};
        check(dev.read(42, out) == 4 && out[0] == 0x5a5a && out[1] == 0x5a5a, "array read");
        std::vector<uint32_t> vec(2);
        check(dev.read(42, vec) == 8 && vec[0] == 0x5a5a5a5a && vec[1] == 0x5a5a5a5a, "vector read");
    } else if (test == "empty") {
        const std::array<uint8_t, 0> tx{};
        std::vector<uint8_t> rx;
        check(dev.write(42, tx) == 0 && dev.read(42, rx) == 0, "empty transfer failed");
        check(reads == 0 && writes == 0, "empty transfer touched bus");
    } else { check(false, "unknown case"); }
}
