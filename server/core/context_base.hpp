/// (c) Koheron

#ifndef __CONTEXT_BASE_HPP__
#define __CONTEXT_BASE_HPP__

#include <server_definitions.hpp>
#include <lib/syslog.hpp>
#include "driver_id.hpp"

#include <string>

namespace koheron {
    template<driver_id id>
    device_t<id>& get_driver();
} // namespace koheron

class ContextBase
{
  public:
    template<class Driver>
    inline Driver& get() const {
      return koheron::get_driver<koheron::driver_id_of<Driver>>();
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