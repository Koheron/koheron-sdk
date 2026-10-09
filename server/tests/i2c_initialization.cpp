#include "server/hardware/i2c_manager.hpp"
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

namespace {
unsigned entries = 0, opens = 0;
bool deny = true;
constexpr auto device = "i2c-regression-device-with-long-name";
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
}
extern "C" DIR* __wrap_opendir(const char* path) {
    check(std::strcmp(path, "/sys/class/i2c-dev") == 0, "wrong discovery path");
    entries = 0;
    return reinterpret_cast<DIR*>(&entries);
}
extern "C" dirent* __wrap_readdir(DIR*) {
    static dirent entry{};
    if (entries++) { return nullptr; }
    std::strcpy(entry.d_name, device);
    return &entry;
}
// ARM's large-file ABI redirects readdir/open to their 64-bit entry points.
extern "C" dirent64* __wrap_readdir64(DIR*) {
    static dirent64 entry{};
    if (entries++) { return nullptr; }
    std::strcpy(entry.d_name, device);
    return &entry;
}
extern "C" int __wrap_closedir(DIR*) { return 0; }
extern "C" int __real_open(const char*, int, ...);
extern "C" int __wrap_open(const char* path, int flags, ...) {
    ++opens;
    check(std::string(path) == std::string("/dev/") + device, "device path lifetime");
    if (deny) { errno = EACCES; return -1; }
    return __real_open("/dev/null", flags);
}
extern "C" int __wrap_open64(const char* path, int flags, ...) {
    return __wrap_open(path, flags);
}
int main() {
    hw::I2cManager manager;
    check(manager.init() == 0, "discovery");
    check(!manager.get(device).is_ok(), "failed open must remain unavailable");
    deny = false;
    auto& opened = manager.get(device);
    check(opened.is_ok(), "open must be retryable");
    check(manager.get(device).is_ok() && opens == 2, "initialized device reopened");
}
