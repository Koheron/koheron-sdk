/// Picoblaze driver
///
/// (c) Koheron

#ifndef __DRIVERS_PICOBLAZE_HPP__
#define __DRIVERS_PICOBLAZE_HPP__

#include <context.hpp>

class Picoblaze
{
  public:
    Picoblaze(Context& ctx)
    : ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , picoram(ctx.mm.get<mem::picoram>())
    {}

    void write_ram(uint32_t address, uint32_t value) {
        picoram.write_reg(address, value);
    }

    void reset() {
        ctl.set_bit<reg::reset, 0>();
        ctl.clear_bit<reg::reset, 0>();
    }

    void set_input(uint32_t value) {
        ctl.write<reg::in_port>(value);
    }

    uint32_t get_output() {
        return sts.read<reg::out_port>();
    }

  private:
    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::status>& sts;
    hw::Memory<mem::picoram>& picoram;
};

#endif // __DRIVERS_PICOBLAZE_HPP__
