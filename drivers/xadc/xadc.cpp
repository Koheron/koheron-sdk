/// (c) Koheron 

#include "xadc.hpp"

#include <algorithm>
#include <iterator>

Xadc::Xadc(Klib::DevMem& dvm_)
: dvm(dvm_)
{
    xadc_map = dvm.AddMemoryMap(XADC_ADDR, XADC_RANGE);
}

bool is_valid_channel(uint32_t channel)
{
    constexpr uint32_t valid_channels[] = {0, 1, 8, 9};

    return std::any_of(std::begin(valid_channels), std::end(valid_channels), 
        [&](uint32_t i) { 
            return i == channel;
        });
}

int Xadc::set_channel(uint32_t channel_0_, uint32_t channel_1_)
{
    if (!is_valid_channel(channel_0_) || !is_valid_channel(channel_1_))
        return -1;

    channel_0 = channel_0_;
    channel_1 = channel_1_;

    uint32_t val = (1 << channel_0) + (1 << channel_1);
    dvm.write32(xadc_map, SET_CHAN_OFF, val);
    return 0;
}

int Xadc::set_averaging(uint32_t n_avg)
{
    uint32_t reg;

    switch (n_avg) {
      case 1:
        reg = 0x0000;
        break;
      case 4:
        reg = 0x1000;
        break;
      case 64:
        reg = 0x2000;
        break;
      case 256:
        reg = 0x3000;
        break;
      default:
        return -1;
    }

    dvm.write32(xadc_map, AVG_OFF, reg);
    return 0;
}

int Xadc::read(uint32_t channel)
{
    if (channel != channel_0 && channel != channel_1)
        return -1;

    return dvm.read32(xadc_map, READ_OFF + 4*channel);
}

