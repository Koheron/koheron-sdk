/// Laser development kit driver
///
/// (c) Koheron

#ifndef __DRIVERS_LASER_HPP__
#define __DRIVERS_LASER_HPP__

#include <tuple>

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>
#include <drivers/addresses.hpp>
#include <drivers/xadc.hpp>
#include <drivers/gpio.hpp>

// XADC channels
#define LASER_POWER_CHANNEL   1
#define LASER_CURRENT_CHANNEL 8

#define MAX_LASER_CURRENT 50.0 // mA

class Laser
{
  public:
    Laser(Klib::DevMem& dev_mem_);
    ~Laser();
    
    //> \description Open the device
    //> \io_type WRITE
    //> \status ERROR_IF_NEG
    //> \on_error Cannot open BASE device
    //> \flag AT_INIT
    int Open();
    
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
    
    // Memory maps IDs
    Klib::MemMapID config_map;
};

#endif // __DRIVERS_LASER_HPP__
