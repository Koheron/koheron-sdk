/// Server system log
/// (c) Koheron

#ifndef __SERVER_RUNTIME_SYSLOG_HPP__
#define __SERVER_RUNTIME_SYSLOG_HPP__

#include <array>
#include <string_view>
#include <type_traits>
#include <utility>
#include <filesystem>
#include <format>

enum severity {
    PANIC,    ///< When Server is not functional anymore
    CRITICAL, ///< When an error results in session crash
    ERROR,    ///< Typically when a command execution failed
    WARNING,
    INFO,
    DEBUG,
    syslog_severity_num
};

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
}

struct Severity {
    int priority;
    std::string_view label;
};

static constexpr std::array<Severity, syslog_severity_num> Severities{{
    {PANIC,    "PANIC"},
    {CRITICAL, "CRITICAL"},
    {ERROR,    "ERROR"},
    {WARNING,  "WARNING"},
    {INFO,     "INFO"},
    {DEBUG,    "DEBUG"},
}};

template<int S>
inline constexpr std::string_view severity_msg_v = Severities[S].label;

template<int S=INFO, typename... Args>
void print(std::string_view msg, Args&&... args) {
    static_assert(S >= 0 && S < syslog_severity_num, "Invalid logging level");

    if constexpr (S <= WARNING) {
        if constexpr (sizeof...(Args) == 0) {
            std::fprintf(stderr, "%s: %s", severity_msg_v<S>.data(), msg.data());
        } else {
            std::fprintf(stderr, "%s: ", severity_msg_v<S>.data());
            std::fprintf(stderr, msg.data(), std::forward<Args>(args)...);
        }

        std::fflush(stderr);
    } else if constexpr (S == INFO) {
        if constexpr (sizeof...(Args) == 0) {
            std::fprintf(stdout, "%s", msg.data());
        } else {
            std::fprintf(stdout, msg.data(), std::forward<Args>(args)...);
        }

        std::fflush(stdout);
    } else {
        if constexpr (config::log::verbose) { // Log debug
            if constexpr (sizeof...(Args) == 0) {
                std::fprintf(stdout, "%s", msg.data());
            } else {
                std::fprintf(stdout, msg.data(), std::forward<Args>(args)...);
            }

            std::fflush(stdout);
        }
    }

}

template<int S=INFO, typename... Args>
void print_fmt(std::format_string<Args...> fmt, Args&&... args) {
    print<S>(std::format(fmt, std::forward<Args>(args)...));
}

} // namespace koheron

#endif // __SERVER_RUNTIME_SYSLOG_HPP__
