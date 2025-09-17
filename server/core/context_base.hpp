/// (c) Koheron

#ifndef __CONTEXT_BASE_HPP__
#define __CONTEXT_BASE_HPP__

#include <server_definitions.hpp>
#include <syslog.hpp>

#include <string>

namespace koheron {
    class DriverManager;
    class Server;
} // namespace koheron

class ContextBase
{
  private:
    koheron::DriverManager *driver_manager;
    koheron::SysLog *syslog;

    void set_driver_manager(koheron::DriverManager *driver_manager_) {
        driver_manager = driver_manager_;
    }

    void set_syslog(koheron::SysLog *syslog_) {
        syslog = syslog_;
    }

  public:
    template<class Driver>
    Driver& get() const;

    template<int severity, typename... Args>
    void log(const char *msg, Args&&... args) {
        syslog->print<severity>(msg, std::forward<Args>(args)...);
    }

    template<int severity, typename... Args>
    void logf(std::format_string<Args...> fmt, Args&&... args) {
        syslog->print_fmt<severity>(fmt, std::forward<Args>(args)...);
    }

  protected:
    virtual int init() { return 0; }

friend class koheron::DriverManager;
friend class koheron::Server;
};

#endif // __CONTEXT_BASE_HPP__