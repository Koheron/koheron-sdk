/// Monitor driver
///
/// (c) Koheron

#ifndef __DRIVERS_MONITOR_HPP__
#define __DRIVERS_MONITOR_HPP__

#include <context.hpp>

class Monitor
{
  public:
    Monitor(Context& ctx)
    : ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , xadc(ctx.mm.get<mem::xadc>())
    {}

    float get_temperature() {
        return (xadc.read<0x200>() * 503.975) / 65356 - 273.15;
    }

  private:
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::xadc>& xadc;
};

#endif // __DRIVERS_MONITOR_HPP__
