/// Fast DAC driver
///
/// (c) Koheron

#ifndef __DRIVERS_DAC_HPP__
#define __DRIVERS_DAC_HPP__

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/wr_register.hpp>
#include <drivers/addresses.hpp>

class Dac
{
  public:
    Dac(Klib::DevMem& dev_mem_);
    ~Dac();

    int Open(uint32_t dac_wfm_size_);

    #pragma tcp-server exclude
    void Close();

    void reset();

    #pragma tcp-server write_array arg{data} arg{len}
    void set_dac_buffer(const uint32_t *data, uint32_t len);

    void reset_acquisition();

    enum Status {
        CLOSED,
        OPENED,
        FAILED
    };

    #pragma tcp-server is_failed
    bool IsFailed() const {return status == FAILED;}

  private:
    Klib::DevMem& dev_mem;
    int status;

    // Number of point in the DAC waveform
    uint32_t dac_wfm_size;

    // Memory maps IDs:
    Klib::MemMapID config_map;
    Klib::MemMapID dac_map;
};

#endif // __DRIVERS_DAC_HPP__
