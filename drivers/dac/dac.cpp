/// (c) Koheron

#include "dac.hpp"

Dac::Dac(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
{
    dac_wfm_size = 0;
    status = CLOSED;
}

Dac::~Dac()
{
    Close();
}

int Dac::Open(uint32_t dac_wfm_size_)
{  
    // Reopening if waveform size changed
    if (status == OPENED && dac_wfm_size_ != dac_wfm_size)
        Close();

    if (status == CLOSED) {
        dac_wfm_size = dac_wfm_size_;

        auto ids = dev_mem.RequestMemoryMaps<2>({{
            { CONFIG_ADDR, CONFIG_RANGE },
            { DAC_ADDR   , DAC_RANGE    }
        }});

        if (dev_mem.CheckMapIDs(ids) < 0) {
            status = FAILED;
            return -1;
        }

        config_map = ids[0];
        dac_map    = ids[1];

        status = OPENED;
        reset();
    }

    return 0;
}

void Dac::Close()
{
    if (status == OPENED) {
        reset();
        status = CLOSED;
    }
}

void Dac::reset()
{
    assert(status == OPENED);
    // Config
    //Klib::ClearBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 0);
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 1);
    Klib::SetBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 0);
}

void Dac::set_dac_buffer(const uint32_t *data, uint32_t len)
{
    for (uint32_t i=0; i<len; i++)
        Klib::WriteReg32(dev_mem.GetBaseAddr(dac_map) + sizeof(uint32_t) * i, data[i]);
}

void Dac::reset_acquisition()
{
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 1);
    Klib::SetBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 1);
}