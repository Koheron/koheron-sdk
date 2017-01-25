/// XADC driver
///
/// http://www.xilinx.com/support/documentation/ip_documentation/xadc_wiz/v3_0/pg091-xadc-wiz.pdf
/// http://www.xilinx.com/support/documentation/user_guides/ug480_7Series_XADC.pdf
///
/// (c) Koheron

#ifndef __DRIVERS_XADC_HPP__
#define __DRIVERS_XADC_HPP__

#include <algorithm>
#include <iterator>

#include <context.hpp>

// Offsets
// Set by Xilinx IP
#define XADC_SET_CHAN_OFF  0x324
#define XADC_AVG_EN_OFF    0x32C
#define XADC_READ_OFF      0x240
#define XADC_CFG0_OFF      0x300
#define XADC_CFG1_OFF      0x304
#define XADC_CFG2_OFF      0x308

class Xadc
{
  public:
    Xadc(Context& ctx)
    : xadc(ctx.mm.get<mem::xadc>())
    {}

    uint32_t set_channel(uint32_t channel_0_, uint32_t channel_1_) {
        if (!is_valid_channel(channel_0_) || !is_valid_channel(channel_1_))
            return -1;

        channel_0 = channel_0_;
        channel_1 = channel_1_;

        uint32_t val = (1 << channel_0) + (1 << channel_1);
        xadc.write<XADC_SET_CHAN_OFF>(val);
        return 0;
    }
    
    void enable_averaging() {
        xadc.write<XADC_AVG_EN_OFF>((1 << channel_0) + (1 << channel_1));
    }

    uint32_t set_averaging(uint32_t n_avg) {
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
            return -1;
        }
        xadc.write_mask<XADC_CFG0_OFF, mask>(avg << 12);
        return 0;
    }

    uint32_t read(uint32_t channel) {
        if (channel != channel_0 && channel != channel_1)
            return -1;
        return xadc.read_reg(XADC_READ_OFF + 4 * channel);
    }

  private:
    Memory<mem::xadc>& xadc;

    uint32_t channel_0 = 1;
    uint32_t channel_1 = 8;

    bool is_valid_channel(uint32_t channel) {
        constexpr uint32_t valid_channels[] = {0, 1, 8, 9};

        return std::any_of(std::begin(valid_channels), std::end(valid_channels), 
            [&](uint32_t i) { 
                return i == channel;
            });
    }
};

#endif //__DRIVERS_XADC_HPP__
