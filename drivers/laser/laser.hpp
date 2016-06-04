/// Laser development kit driver
///
/// (c) Koheron

#ifndef __DRIVERS_LASER_HPP__
#define __DRIVERS_LASER_HPP__

#include <tuple>

#include <drivers/lib/dev_mem.hpp>
#include <drivers/addresses.hpp>
#include <drivers/xadc.hpp>
#include <drivers/gpio.hpp>

// XADC channels
#define LASER_POWER_CHANNEL   1
#define LASER_CURRENT_CHANNEL 8

# define LASER_ENABLE_PIN 5

#define MAX_LASER_CURRENT 50.0 // mA

class Laser
{
  public:
    Laser(Klib::DevMem& dvm_);
    ~Laser() {if (status == OPENED) reset();}
    
    int Open() {return status == FAILED ? -1 : 0;}
       
    void reset();

    uint32_t get_laser_current() {return xadc.read(LASER_CURRENT_CHANNEL);}
    uint32_t get_laser_power()   {return xadc.read(LASER_POWER_CHANNEL);}

    std::tuple<uint32_t, uint32_t> get_monitoring() {
        return std::make_tuple(get_laser_current(), get_laser_power());
    }

    // Laser enable on pin DIO7_P
    void start_laser() {gpio.clear_bit(LASER_ENABLE_PIN, 2);}
    void stop_laser()  {gpio.set_bit(LASER_ENABLE_PIN, 2);}

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
    Klib::DevMem& dvm;
    Xadc xadc;
    Gpio gpio;
        
    int status;
    
    // Memory maps IDs
    Klib::MemMapID config_map; // Config is required for the PWMs
};

#endif // __DRIVERS_LASER_HPP__
