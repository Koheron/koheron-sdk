/// (c) Koheron 

#include "xadc.hpp"

#include <algorithm>
#include <iterator>

Xadc::Xadc(DevMem& dvm_)
: dvm(dvm_)
{
    xadc_map = dvm.add_memory_map(XADC_ADDR, XADC_RANGE);
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
    dvm.write<SET_CHAN_OFF>(xadc_map, val);
    return 0;
}

int Xadc::set_averaging(uint32_t n_avg)
{
    uint32_t avg;
    uint32_t mask = (1 << 12) + (1 << 13);

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
    dvm.write32_mask(xadc_map, XADC_CFG0_OFF, (avg << 12), mask);
    return 0;
}

int Xadc::read(uint32_t channel)
{
    if (channel != channel_0 && channel != channel_1)
        return -1;
    return dvm.read_offset(xadc_map, READ_OFF + 4*channel);
}

