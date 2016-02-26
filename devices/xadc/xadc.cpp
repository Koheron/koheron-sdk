/// (c) Koheron 

#include "xadc.hpp"

#include <algorithm>
#include <iterator>

Xadc::Xadc(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
{
    status = CLOSED;
}

Xadc::~Xadc()
{
    Close();
}

int Xadc::Open(uint32_t map_size)
{
    if (status == CLOSED) {
        dev_num = dev_mem.AddMemoryMap(XADC_ADDR, map_size);
        
        if (static_cast<int>(dev_num) < 0) {
            status = FAILED;
            return -1;
        }
        
        status = OPENED;
    }
    
    return 0;
}

void Xadc::Close()
{
    if (status == OPENED) {
        dev_mem.RmMemoryMap(dev_num);
        status = CLOSED;
    }
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
    Klib::WriteReg32(dev_mem.GetBaseAddr(dev_num) + SET_CHAN_OFF, val);
    return 0;
}

void Xadc::enable_averaging()
{
    uint32_t val = (1 << channel_0) + (1 << channel_1);
    Klib::WriteReg32(dev_mem.GetBaseAddr(dev_num) + AVG_EN_OFF, val);
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

    Klib::WriteReg32(dev_mem.GetBaseAddr(dev_num) + AVG_OFF, reg);
    return 0;
}

int Xadc::read(uint32_t channel)
{
    if (channel != channel_0 && channel != channel_1)
        return -1;

    return Klib::ReadReg32(dev_mem.GetBaseAddr(dev_num) + READ_OFF + 4*channel);
}

