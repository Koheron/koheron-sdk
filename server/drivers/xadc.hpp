/// XADC driver
/// (c) Koheron

#ifndef __DRIVERS_XADC_HPP__
#define __DRIVERS_XADC_HPP__

#include <algorithm>
#include <iterator>

#include <context.hpp>

/// http://www.xilinx.com/support/documentation/user_guides/ug480_7Series_XADC.pdf
namespace Xadc_regs {
    constexpr uint32_t set_chan = 0x324;
    constexpr uint32_t avg_en = 0x32C;
    constexpr uint32_t read = 0x240;
    constexpr uint32_t config0 = 0x300;
}

class Xadc
{
  public:
    Xadc(Context& ctx)
    : xadc(ctx.mm.get<mem::xadc>())
    {
      enable_averaging();
      set_averaging(256);
    }

    void enable_averaging() {
        xadc.write<Xadc_regs::avg_en>((1 << channel_0) + (1 << channel_1));
    }

    void set_averaging(uint32_t n_avg) {
        uint32_t avg;
        constexpr uint32_t mask = (1 << 12) + (1 << 13);
        switch (n_avg) {
          case 1:
            avg = 0;
            break;
          case 4:
            avg = 1;
            break;
          case 64:
            avg = 2;
            break;
          case 256:
            avg = 3;
            break;
          default:
            avg = 0;
        }
        xadc.write_mask<Xadc_regs::config0, mask>(avg << 12);
    }

    uint32_t read(uint32_t channel) {
        if (channel != channel_0 && channel != channel_1) {
            return 0;
        }
        return xadc.read_reg(Xadc_regs::read + 4 * channel);
    }

  private:
    Memory<mem::xadc>& xadc;

    const uint32_t channel_0 = 1;
    const uint32_t channel_1 = 8;

};

#endif //__DRIVERS_XADC_HPP__
