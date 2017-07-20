/// Server system log
///
/// (c) Koheron

#ifndef __KOHERON_SYSLOG_HPP__
#define __KOHERON_SYSLOG_HPP__

#include "config.hpp"

#include <memory>
#include <string>
#include <cstring>
#include <tuple>

#include <syslog.h>

#include "string_utils.hpp"

/// Severity of the message
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

static constexpr auto log_array = koheron::make_array(
    std::make_tuple(LOG_ALERT, str_const("KOHERON PANIC")),
    std::make_tuple(LOG_CRIT, str_const("KOHERON CRITICAL")),
    std::make_tuple(LOG_ERR, str_const("KOHERON ERROR")),
    std::make_tuple(LOG_WARNING, str_const("KOHERON WARNING")),
    std::make_tuple(LOG_NOTICE, str_const("KOHERON INFO")),
    std::make_tuple(LOG_DEBUG, str_const("KOHERON DEBUG"))
);

template<int severity>
constexpr str_const severity_msg = std::get<1>(std::get<severity>(log_array));

struct SysLog
{
    template<int severity, typename... Args>
    void print(const char *msg, Args&&... args);

  private:
    SysLog() {
        if (config::log::syslog) {
            setlogmask(LOG_UPTO(LOG_NOTICE));
            openlog("koheron-server", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
        }
    }

    // This cannot be done in the destructor
    // since it is called after the "delete config"
    // at the end of the main()
    void close() {
        if (config::log::syslog) {
            print<INFO>("Close syslog ...\n");
            closelog();
        }
    }

    // High severity (Panic, ..., Warning) => stderr
    template<int severity, typename... Args>
    std::enable_if_t< severity <= WARNING && config::log::use_stderr, void >
    print_msg(const char *message, Args... args) {
        koheron::fprintf(stderr, (severity_msg<severity>.to_string()
                                    + ": " + std::string(message)).c_str(),
                            std::forward<Args>(args)...);
    }

    template<int severity, typename... Args>
    std::enable_if_t< severity <= WARNING && !config::log::use_stderr, void >
    print_msg(const char *, Args...) {}

    // Low severity (Info, Debug) => stdout if verbose
    template<int severity, typename... Args>
    std::enable_if_t< severity >= INFO && config::log::verbose, void >
    print_msg(const char *message, Args&&... args) {
        koheron::printf(message, std::forward<Args>(args)...);
    }

    template<int severity, typename... Args>
    std::enable_if_t< severity >= INFO && !config::log::verbose, void >
    print_msg(const char *, Args&&...) {}

    template<int severity>
    static constexpr int to_priority = std::get<0>(std::get<severity>(log_array));

    static_assert(to_priority<PANIC> == LOG_ALERT, "");
    static_assert(to_priority<INFO> == LOG_NOTICE, "");

    template<int severity, typename... Args>
    std::enable_if_t< severity <= INFO && config::log::syslog, void >
    call_syslog(const char *message, Args&&... args) {
        syslog<to_priority<severity>>(message, std::forward<Args>(args)...);
    }

    // We don't send debug messages to the system log
    template<int severity, typename... Args>
    std::enable_if_t< severity >= DEBUG, int >
    call_syslog(const char *, Args&&...) {
        return 0;
    }

friend class Server;
friend class SessionManager;
};

template<int severity, typename... Args>
void SysLog::print(const char *msg, Args&&... args)
{
    static_assert(severity <= syslog_severity_num, "Invalid logging level");
    print_msg<severity>(msg, std::forward<Args>(args)...);
    call_syslog<severity>(msg, std::forward<Args>(args)...);
}

} // namespace koheron

#endif // __KOHERON_SYSLOG_HPP__
