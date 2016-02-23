/// Blink bitstream driver
///
/// (c) Koheron

#ifndef __DRIVERS_BLINK_HPP__
#define __DRIVERS_BLINK_HPP__

#include <tuple>
#include <array>

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>
#include <drivers/addresses.hpp>

//> \description Blink driver
class Blink
{
  public:
    Blink(Klib::DevMem& dev_mem_);
    ~Blink();
    
    //> \io_type WRITE
    //> \flag AT_INIT
    int Open(uint32_t dac_wfm_size_);
    
    void Close();

    //> \io_type READ
    std::array<uint32_t, BITSTREAM_ID_SIZE> get_bitstream_id();
    
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
    std::array<uint32_t, BITSTREAM_ID_SIZE> bitstream_id;
    
    // Memory maps IDs:
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
};

#endif // __DRIVERS_Blink_HPP__
