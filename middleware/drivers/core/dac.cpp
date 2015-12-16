/// (c) Koheron

#include "dac.hpp"

Dac::Dac(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
{
    waveform_size = 0;
    status = CLOSED;
}
 
Dac::~Dac()
{
    Close();
}

int Dac::Open(uint32_t waveform_size_)
{
    // Reopening
    if(status == OPENED && waveform_size_ != waveform_size) {
        Close();
    }

    if(status == CLOSED) {
        waveform_size = waveform_size_;
        
        dac_map = dev_mem.AddMemoryMap(DAC_ADDR,
                                       (waveform_size/1024)*MAP_SIZE);

        if(static_cast<int>(dac_map) < 0) {
            status = FAILED;
            return -1;
        }

        status = OPENED;
    }

    return 0;
}

void Dac::Close()
{
    if(status == OPENED) {
        dev_mem.RmMemoryMap(dac_map);
        status = CLOSED;
    }
}

#define ONE_VAL             16384 // 2^14
#define ONE_VAL_MINUS_ONE   16383 // 2^14 - 1
#define ZERO_VAL            8192  // 8192

uint32_t _flt_to_integer(float dac_float)
{
    uint32_t dac_int = 0;

    if (dac_float < -1)
        dac_int = 0;
    else if (dac_float >= 1)
        dac_int = ONE_VAL_MINUS_ONE;
    else
        dac_int = (uint32_t(ZERO_VAL * dac_float + ZERO_VAL)) % ONE_VAL + ZERO_VAL;

    return dac_int;
}

void Dac::set_dac_constant(float dac_1_float, float dac_2_float)
{
    uint32_t dac_int = _flt_to_integer(dac_1_float) + (_flt_to_integer(dac_2_float) << 16);

    for (uint32_t i = 0; i < waveform_size; i++)
        Klib::WriteReg32(dev_mem.GetBaseAddr(dac_map) + sizeof(uint32_t)*i, dac_int);
}

void Dac::set_dac(const uint32_t* buffer, size_t len)
{
    const float *buffer_flt = reinterpret_cast<const float*>(buffer);

    for (uint32_t i = 0; i < waveform_size; i++) {
        uint32_t dac_int = _flt_to_integer(buffer_flt[i])
                           + (_flt_to_integer(buffer_flt[i+waveform_size]) << 16);

        Klib::WriteReg32(dev_mem.GetBaseAddr(dac_map)+sizeof(uint32_t)*i, dac_int);
    }
}

