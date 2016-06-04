/// Laser development kit driver
///
/// (c) Koheron

#ifndef __DRIVERS_LASER_HPP__
#define __DRIVERS_LASER_HPP__

#include <tuple>

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/wr_register.hpp>
#include <drivers/addresses.hpp>
#include <drivers/xadc.hpp>
#include <drivers/gpio.hpp>
#include <drivers/at93c46d.hpp>

#include <thread>
#include <chrono>

// XADC channels
#define LASER_POWER_CHANNEL   1
#define LASER_CURRENT_CHANNEL 8

#define MAX_LASER_CURRENT 50.0 // mA

class Laser
{
  public:
    Laser(Klib::DevMem& dvm_);
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

    uint32_t is_laser_present() {
        eeprom.write_enable();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        eeprom.write(0, 42);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return eeprom.read(0);
    };
    
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
    At93c46d eeprom;
        
    int status;
    
    // Memory maps IDs
    Klib::MemMapID config_map;
};

#endif // __DRIVERS_LASER_HPP__
