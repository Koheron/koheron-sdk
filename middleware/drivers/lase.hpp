/// Laser development kit driver
///
/// (c) Koheron

#ifndef __DRIVERS_LASE_HPP__
#define __DRIVERS_LASE_HPP__

#include <tuple>

#include "dev_mem.hpp"
#include "wr_register.hpp"
#include "xadc.hpp"
#include "gpio.hpp"
#include "addresses.hpp"

// XADC channels
#define LASER_POWER_CHANNEL   1
#define LASER_CURRENT_CHANNEL 8

#define MAX_LASER_CURRENT 50.0 // mA

//> \description Laser development kit driver
class Lase
{
  public:
    Lase(Klib::DevMem& dev_mem_);
    ~Lase();
    
    //> \description Open the device
    //> \io_type WRITE
    //> \status ERROR_IF_NEG
    //> \on_error Cannot open LASE device
    //> \flag AT_INIT
    int Open(uint32_t dac_wfm_size_);
    
    void Close();
    
    //> \description Reset to default state
    //> \io_type WRITE
    void reset();
    
    //> \description Laser current monitoring
    //> \io_type READ
    uint32_t get_laser_current();
    
    //> \description Laser power monitoring
    //> \io_type READ
    uint32_t get_laser_power();
    
    //> \description Send all monitoring data at once
    //> \io_type READ
    std::tuple<uint32_t, uint32_t> get_monitoring();
    
    //> \io_type WRITE
    void start_laser();
    
    //> \io_type WRITE
    void stop_laser();
    
    //> \param current Laser current in mA
    //> \io_type WRITE
    void set_laser_current(float current);

    //> \io_type WRITE_ARRAY param=>data param=>len
    void set_dac_buffer(const uint32_t *data, uint32_t len);
    
    //> \io_type READ
    uint32_t get_bitstream_id();
    
    //> \io_type WRITE
    void set_led(uint32_t value);
    
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
    Xadc xadc;
    Gpio gpio;
        
    int status;
    
    // Number of point in the DAC waveform
    uint32_t dac_wfm_size;
    
    // Memory maps IDs:
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID dac_map;
};

#endif // __DRIVERS_LASE_HPP__
