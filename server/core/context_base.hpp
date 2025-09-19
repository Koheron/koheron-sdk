/// (c) Koheron

#ifndef __CONTEXT_BASE_HPP__
#define __CONTEXT_BASE_HPP__

#include <lib/syslog.hpp>
#include "driver_id.hpp"

class ContextBase
{
  public:
    template<class Driver>
    inline Driver& get() const {
        return koheron::get_driver<Driver>();
    }

    template<int severity, typename... Args>
    void log(const char *msg, Args&&... args) {
        koheron::print<severity>(msg, std::forward<Args>(args)...);
    }

    template<int severity, typename... Args>
    void logf(std::format_string<Args...> fmt, Args&&... args) {
        koheron::print_fmt<severity>(fmt, std::forward<Args>(args)...);
    }

  protected:
    virtual int init() { return 0; }
};

#endif // __CONTEXT_BASE_HPP__