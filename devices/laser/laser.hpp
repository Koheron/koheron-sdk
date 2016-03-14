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
    
    int Open();
    
    #pragma tcp-server exclude
    void Close();
    
    void reset();
    uint32_t get_laser_current();
    uint32_t get_laser_power();
    std::tuple<uint32_t, uint32_t> get_monitoring();
    void start_laser();
    void stop_laser();
    void set_laser_current(float current);
    
    enum Status {
        CLOSED,
        OPENED,
        FAILED
    };

    #pragma tcp-server is_failed
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
