// Real Memory/MemoryManager/UIO code with synthetic mappings and device opens.
#include "server/hardware/memory_manager.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string_view>

namespace {
std::set<int> descriptors;
std::map<void*, std::size_t> mappings;
unsigned opens = 0, maps = 0;
unsigned failed_maps = 0;
bool fail_open = false, fail_custom = false;
std::string uio_path;
void* last_mapping = nullptr;
std::size_t last_length = 0;
off64_t last_offset = 0;
void check(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::abort(); }
}
void clean() {
    check(descriptors.empty(), "descriptor leaked");
    check(mappings.empty(), "mapping leaked");
}
template<MemID ID> void opened(hw::Memory<ID>& memory) {
    const int fd = memory.open();
    check(fd >= 0 && memory.is_open(), "open failed");
    // Check interception before any memory access, including on large-file ARM builds.
    check(descriptors.contains(fd), "device open bypassed fixture");
    check(std::any_of(mappings.begin(), mappings.end(), [&](const auto& entry) {
        const auto start = reinterpret_cast<uintptr_t>(entry.first);
        return memory.base_addr() >= start && memory.base_addr() - start < entry.second;
    }), "mapping bypassed fixture");
    memory.template write<0>(0x12345678u);
    check(memory.template read<0>() == 0x12345678u, "register access failed");
}
template<MemID ID> void retry_mapping() {
    hw::Memory<ID> memory;
    failed_maps = 1;
    check(memory.open() < 0, "mmap failure accepted");
    check(!memory.is_open() && memory.base_addr() == 0, "failed map published address");
    clean();
    opened(memory);
}
template<MemID ID> void reopen() {
    hw::Memory<ID> memory;
    opened(memory);
    const auto address = memory.base_addr();
    const int fd = *descriptors.begin();
    const auto old_opens = opens, old_maps = maps;
    check(memory.open() == fd, "reopen changed descriptor");
    check(memory.base_addr() == address, "reopen invalidated pointers");
    check(opens == old_opens && maps == old_maps, "reopen allocated resources");
    check(memory.template read<0>() == 0x12345678u, "reopen lost register contents");
}
}

extern "C" int __real_open(const char*, int, ...);
extern "C" int __real_close(int);
extern "C" void* __real_mmap64(void*, std::size_t, int, int, int, off64_t);
extern "C" int __real_munmap(void*, std::size_t);
extern "C" int __wrap_open(const char* path, int flags, ...) {
    const std::string_view name{path};
    check(name == "/dev/mem" || name == "/fixture/custom" || name == uio_path,
          "unexpected device open");
    ++opens;
    if (fail_open || (fail_custom && name == "/fixture/custom")) { errno = EACCES; return -1; }
    const int fd = __real_open("/dev/null", flags);
    check(fd >= 0, "cannot open synthetic device");
    descriptors.insert(fd);
    return fd;
}
extern "C" int __wrap_open64(const char* path, int flags, ...) {
    return __wrap_open(path, flags);
}
extern "C" int __wrap_close(int fd) {
    check(descriptors.erase(fd) == 1, "closed an unowned descriptor");
    return __real_close(fd);
}
extern "C" void* __wrap_mmap64(void*, std::size_t length, int protection, int flags, int fd, off64_t offset) {
    check(descriptors.contains(fd), "mapped an unowned descriptor");
    check(flags == MAP_SHARED, "mapping flags changed");
    ++maps;
    if (failed_maps) { --failed_maps; errno = ENOMEM; return MAP_FAILED; }
    void* address = __real_mmap64(nullptr, length, protection, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    check(address != MAP_FAILED, "synthetic mapping failed");
    mappings.emplace(address, length);
    last_mapping = address; last_length = length; last_offset = offset;
    return address;
}
extern "C" void* __wrap_mmap(void* address, std::size_t length, int protection, int flags, int fd, __off_t offset) {
    return __wrap_mmap64(address, length, protection, flags, fd, offset);
}
extern "C" int __wrap_munmap(void* address, std::size_t length) {
    const auto found = mappings.find(address);
    check(found != mappings.end(), "unmapped an unowned mapping");
    check(length == found->second, "unmap length differs from mapped length");
    mappings.erase(found);
    return __real_munmap(address, length);
}

int main(int argc, char** argv) {
    check(argc == 3, "fixture root and case required");
    uio_path = std::string(argv[1]) + "/dev/uio0";
    const std::string_view test{argv[2]};
    if (test == "devmem") {
        hw::Memory<0> memory;
        opened(memory);
        check(last_offset == 0x1000 && last_length == 8192, "unaligned physical mapping geometry");
        check(memory.base_addr() == reinterpret_cast<uintptr_t>(last_mapping) + 0x80, "physical page offset lost");
    } else if (test == "custom") {
        hw::Memory<1> memory;
        opened(memory);
        check(last_offset == 0 && last_length == 4096, "custom mapping used physical page offset");
        check(memory.base_addr() == reinterpret_cast<uintptr_t>(last_mapping), "custom mapping shifted");
    } else if (test == "uio") {
        hw::Memory<2> memory;
        opened(memory);
        check(last_offset == 0 && last_length == 4096, "UIO map geometry");
    } else if (test == "uio_fallback") {
        hw::Memory<3> memory;
        opened(memory);
        check(last_offset == 0x3000, "UIO fallback physical offset");
    } else if (test == "custom_fallback") {
        fail_custom = true;
        hw::Memory<1> memory;
        opened(memory);
        check(last_offset == 0x1000 && last_length == 8192, "custom fallback geometry");
    } else if (test == "mmap_failure") {
        retry_mapping<0>(); clean();
        retry_mapping<1>(); clean();
        retry_mapping<2>(); clean();
        retry_mapping<3>();
    } else if (test == "open_failure") {
        fail_open = true;
        hw::Memory<3> memory;
        check(memory.open() < 0, "failed open accepted");
        check(!memory.is_open() && memory.base_addr() == 0, "failed open published address");
        clean();
        fail_open = false;
        opened(memory);
    } else if (test == "reopen") {
        reopen<0>(); clean();
        reopen<1>(); clean();
        reopen<2>(); clean();
        reopen<3>();
    } else if (test == "cloexec") {
        hw::MemoryManager manager;
        check(manager.open() == 0, "manager open failed");
        for (int fd : descriptors) {
            check((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0, "descriptor inherited across exec");
        }
    } else if (test == "manager_retry") {
        hw::MemoryManager manager;
        failed_maps = 1;
        check(manager.open() < 0, "manager ignored map failure");
        check(descriptors.size() == 3 && mappings.size() == 3, "partial initialization leaked resources");
        const auto address = manager.get<1>().base_addr();
        const auto old_maps = maps;
        check(manager.open() == 0, "manager retained stale failure");
        check(maps == old_maps + 1 && manager.get<1>().base_addr() == address, "retry remapped successful memory");
    } else { check(false, "unknown case"); }
    clean();
    return 0;
}
