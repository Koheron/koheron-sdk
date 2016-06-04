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

#define EEPROM_CURRENT_ADDR 0
#define TEST_EEPROM_ADDR 63

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

    bool is_laser_present() {
        eeprom.write_enable();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        eeprom.write(TEST_EEPROM_ADDR, 42);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return eeprom.read(TEST_EEPROM_ADDR) == 42;
    }

    void save_config() {
        uint32_t current = dvm.read32(config_map, PWM3_OFF);
        eeprom.write(EEPROM_CURRENT_ADDR, current);
    }

    uint32_t load_config() {
        uint32_t current = eeprom.read(EEPROM_CURRENT_ADDR);
        dvm.write32(config_map, PWM3_OFF, current);
        return current;
    }
    
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
