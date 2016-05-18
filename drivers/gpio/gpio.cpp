/// (c) Koheron

#include "gpio.hpp"

Gpio::Gpio(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
{
    status = CLOSED;
}

int Gpio::Open()
{
    if (status == CLOSED) {
        auto ids = dev_mem.RequestMemoryMaps<1>({{
            { GPIO_ADDR, GPIO_RANGE }
        }});

        if (dev_mem.CheckMapIDs(ids) < 0) {
            status = FAILED;
            return -1;
        }

        dev_num = ids[0];
        status = OPENED;
    }

    return 0;
}

inline int get_value_offset(uint32_t channel)
{
    return (channel == 1 ? CHAN1_VALUE_OFF : CHAN2_VALUE_OFF);
}

inline int get_dir_offset(uint32_t channel)
{
    return (channel == 1 ? CHAN1_DIR_OFF : CHAN2_DIR_OFF);
}

void Gpio::set_data(uint32_t channel, uint32_t value)
{
    Klib::WriteReg32(dev_mem.GetBaseAddr(dev_num) + get_value_offset(channel), value);
}

uint32_t Gpio::get_data(uint32_t channel)
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(dev_num) + get_value_offset(channel));
}


void Gpio::set_bit(uint32_t index, uint32_t channel)
{
    if (index <= MAX_BIT_IDX)
        Klib::SetBit(dev_mem.GetBaseAddr(dev_num) + get_value_offset(channel), index);
}

void Gpio::clear_bit(uint32_t index, uint32_t channel)
{
    if (index <= MAX_BIT_IDX)
        Klib::ClearBit(dev_mem.GetBaseAddr(dev_num) + get_value_offset(channel), index);
}

void Gpio::toggle_bit(uint32_t index, uint32_t channel)
{
    if(index <= MAX_BIT_IDX)
        Klib::ToggleBit(dev_mem.GetBaseAddr(dev_num) + get_value_offset(channel), index);
}

void Gpio::set_as_input(uint32_t index, uint32_t channel)
{
    if(index <= MAX_BIT_IDX)
        Klib::SetBit(dev_mem.GetBaseAddr(dev_num) + get_dir_offset(channel), index);
}

void Gpio::set_as_output(uint32_t index, uint32_t channel)
{
    if(index <= MAX_BIT_IDX)
        Klib::ClearBit(dev_mem.GetBaseAddr(dev_num) + get_dir_offset(channel), index);
}