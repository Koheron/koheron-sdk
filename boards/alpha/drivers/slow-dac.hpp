#ifndef __SLOW_DAC_HPP__
#define __SLOW_DAC_HPP__

#include <context.hpp>

class SlowDac
{
  public:
    SlowDac(Context& ctx)
    : ctl(ctx.mm.get<mem::control>())
    {}

    enum regs {
        NO_OPERATION,
        WRITE,
        UPDATE,
        WRITE_UPDATE,
        POWER_UP_DOWN,
        LDAC_MASK,
    };

    void init() {
        ctl.write<reg::slow_dac_ctl>((regs::WRITE_UPDATE << 1) + enable);
        set_dac_value(0, 0 * 65535);
        set_dac_value(1, 0 * 65535);
        set_dac_value(2, 0 * 65535);
        set_dac_value(3, 0 * 65535);
    }

    void set_dac_value(uint32_t channel, uint32_t value) {
        dac_values[channel & 0b11] = value;
        ctl.write<reg::slow_dac_data0>((dac_values[1] << 16) + (dac_values[0] & 0xFFFF));
        ctl.write<reg::slow_dac_data1>((dac_values[3] << 16) + (dac_values[2] & 0xFFFF));
    }

  private:
    Memory<mem::control>& ctl;
    std::array<uint32_t, 4> dac_values;
    uint32_t enable = 1;

};

#endif // __SLOW_DAC_HPP__