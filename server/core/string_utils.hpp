/// (c) Koheron

#ifndef __STRING_UTILS_HPP__
#define __STRING_UTILS_HPP__

#include <cstring>
#include <string>
#include <stdexcept>
#include <syslog.h>

namespace koheron {

// -------------------------------------------------------------------------
// Variadic string formating functions accepting parameter packs
// -------------------------------------------------------------------------

template <typename... Args>
void printf(const char* fmt, Args&&... args) {
    if constexpr (sizeof...(Args) == 0) {
        std::printf("%s", fmt);
    } else {
        std::printf(fmt, std::forward<Args>(args)...);
    }
}

template <typename... Args>
void fprintf(FILE* stream, const char* fmt, Args&&... args) {
    if constexpr (sizeof...(Args) == 0) {
        std::fprintf(stream, "%s", fmt);
    } else {
        std::fprintf(stream, fmt, std::forward<Args>(args)...);
    }
}

template <typename... Args>
int snprintf(char* s, std::size_t n, const char* fmt, Args&&... args) {
    if constexpr (sizeof...(Args) == 0) {
        return std::snprintf(s, n, "%s", fmt);
    } else {
        return std::snprintf(s, n, fmt, std::forward<Args>(args)...);
    }
}

template <int Priority, typename... Args>
void syslog(const char* fmt, Args&&... args) {
    if constexpr (sizeof...(Args) == 0) {
        ::syslog(Priority, "%s", fmt);
    } else {
        ::syslog(Priority, fmt, std::forward<Args>(args)...);
    }
}

} // namespace koheron

#endif // __STRING_UTILS_HPP__