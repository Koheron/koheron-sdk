/// Server system log
/// (c) Koheron

#ifndef __KOHERON_SYSLOG_HPP__
#define __KOHERON_SYSLOG_HPP__

#include "config.hpp"
#include "string_utils.hpp"

#include <array>
#include <format>
#include <string_view>
#include <type_traits>
#include <utility>

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

namespace koheron {

class Server;

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

struct SysLog
{
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

  private:
    SysLog()
    {
        if (config::log::syslog) {
            setlogmask(LOG_UPTO(LOG_NOTICE)); // PANIC..INFO
            openlog("koheron-server", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
        }
    }

    // This cannot be done in the destructor
    // since it is called after the "delete config"
    // at the end of the main()
    void close()
    {
        if (config::log::syslog) {
            print<INFO>("Close syslog ...\n");
            closelog();
        }
    }

  friend class Server;
  friend class SessionManager;
};

} // namespace koheron

#endif // __KOHERON_SYSLOG_HPP__
