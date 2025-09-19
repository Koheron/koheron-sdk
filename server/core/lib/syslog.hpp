/// Server system log
/// (c) Koheron

#ifndef __KOHERON_SYSLOG_HPP__
#define __KOHERON_SYSLOG_HPP__

#include <array>
#include <string_view>
#include <type_traits>
#include <utility>
#include <filesystem>
#include <format>

#include <syslog.h>

enum severity {
    PANIC,    ///< When Server is not functional anymore
    CRITICAL, ///< When an error results in session crash
    ERROR,    ///< Typically when a command execution failed
    WARNING,
    INFO,
    DEBUG,
    syslog_severity_num
};

// Formatter for std::filesystem::path
template <>
struct std::formatter<std::filesystem::path> : std::formatter<std::string> {
    auto format(const std::filesystem::path& p, auto& ctx) const {
        return std::formatter<std::string>::format(p.string(), ctx);
    }
};

namespace koheron {

namespace config::log {
    /// Display messages emitted and received
    constexpr bool verbose = false;

    /// Send error messages to stderr
    constexpr bool use_stderr = true;

    /// Send messages to syslog
    constexpr bool syslog = true;
}

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

struct Severity {
    int priority;
    std::string_view label;
};

static constexpr std::array<Severity, syslog_severity_num> Severities{{
    {LOG_ALERT,   "KOHERON PANIC"},
    {LOG_CRIT,    "KOHERON CRITICAL"},
    {LOG_ERR,     "KOHERON ERROR"},
    {LOG_WARNING, "KOHERON WARNING"},
    {LOG_NOTICE,  "KOHERON INFO"},
    {LOG_DEBUG,   "KOHERON DEBUG"},
}};

template<int S>
inline constexpr std::string_view severity_msg_v = Severities[S].label;

template<int S>
inline constexpr int to_priority_v = Severities[S].priority;

static_assert(to_priority_v<PANIC> == LOG_ALERT);
static_assert(to_priority_v<INFO>  == LOG_NOTICE);

template<int S, typename... Args>
void print(std::string_view msg, Args&&... args)
{
    static_assert(S >= 0 && S < syslog_severity_num, "Invalid logging level");

    if constexpr (S <= WARNING && config::log::use_stderr) {
        const std::string prefixed =
            std::string(severity_msg_v<S>) + ": " + std::string(msg);

        if constexpr (sizeof...(Args) == 0) {
            koheron::fprintf(stderr, "%s", prefixed.c_str());
        } else {
            koheron::fprintf(stderr, prefixed.c_str(), std::forward<Args>(args)...);
        }
    }

    if constexpr (S >= INFO && config::log::verbose) {
        if constexpr (sizeof...(Args) == 0) {
            koheron::printf("%s", std::string(msg).c_str());
        } else {
            koheron::printf(msg.data(), std::forward<Args>(args)...);
        }
    }

    if constexpr (S <= INFO && config::log::syslog) {
        if constexpr (sizeof...(Args) == 0) {
            ::syslog(to_priority_v<S>, "%s", std::string(msg).c_str());
        } else {
            ::syslog(to_priority_v<S>, msg.data(), std::forward<Args>(args)...);
        }
    }
}

template<int S, typename... Args>
void print_fmt(std::format_string<Args...> fmt, Args&&... args)
{
    print<S>(std::format(fmt, std::forward<Args>(args)...));
}

inline void start_syslog() {
    if constexpr (config::log::syslog) {
        setlogmask(LOG_UPTO(LOG_NOTICE)); // PANIC..INFO
        openlog("koheron-server", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
    }
}

inline void stop_syslog() {
    if constexpr (config::log::syslog) {
        print<INFO>("Close syslog ...\n");
        closelog();
    }
}

} // namespace koheron

#endif // __KOHERON_SYSLOG_HPP__
