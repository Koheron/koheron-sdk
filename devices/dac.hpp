/// Fast DAC driver
///
/// (c) Koheron

#ifndef __DRIVERS_DAC_HPP__
#define __DRIVERS_DAC_HPP__

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>
#include <drivers/addresses.hpp>

class Dac
{
  public:
    Dac(Klib::DevMem& dev_mem_);
    ~Dac();

    //> \io_type WRITE
    //> \status ERROR_IF_NEG
    //> \on_error Cannot open DAC device
    //> \flag AT_INIT
    int Open(uint32_t dac_wfm_size_);
    
    void Close();
    
    //> \description Reset to default state
    //> \io_type WRITE
    void reset();

    //> \io_type WRITE_ARRAY param=>data param=>len
    void set_dac_buffer(const uint32_t *data, uint32_t len);

    // XXX (TV) Is it for oscillo ??
    //> \io_type WRITE
    void reset_acquisition();

    enum Status {
        CLOSED,
        OPENED,
        FAILED
    };

    //> \is_failed
    bool IsFailed() const {return status == FAILED;}

  private:
    // Core drivers
    Klib::DevMem& dev_mem;
        
    int status;
    
    // Number of point in the DAC waveform
    uint32_t dac_wfm_size;
    
    // Memory maps IDs:
    Klib::MemMapID config_map;
    Klib::MemMapID dac_map;
};

#endif // __DRIVERS_DAC_HPP__
