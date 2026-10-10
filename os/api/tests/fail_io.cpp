// Host-only fault injection for real installer filesystem operations.
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string_view>
#include <unistd.h>

extern "C" int rename(const char* from, const char* to) {
    const char* fault = std::getenv("NATIVE_IO_FAULT");
    const std::string_view path(from);
    if (fault && ((std::strcmp(fault, "swap") == 0 && path.ends_with("/next")) ||
                  (std::strcmp(fault, "restore") == 0 && path.ends_with("/previous")))) {
        errno = EIO; return -1;
    }
    static const auto real = reinterpret_cast<int (*)(const char*, const char*)>(dlsym(RTLD_NEXT, "rename"));
    return real(from, to);
}

extern "C" ssize_t write(int fd, const void* data, size_t size) {
    if (const char* fault = std::getenv("NATIVE_IO_FAULT"); fault && std::strcmp(fault, "extract") == 0) {
        char descriptor[64], target[4096];
        std::snprintf(descriptor, sizeof(descriptor), "/proc/self/fd/%d", fd);
        const auto length = readlink(descriptor, target, sizeof(target));
        if (length > 0 && std::string_view(target, length).find("/next/") != std::string_view::npos) {
            errno = ENOSPC; return -1;
        }
    }
    static const auto real = reinterpret_cast<ssize_t (*)(int, const void*, size_t)>(dlsym(RTLD_NEXT, "write"));
    return real(fd, data, size);
}
