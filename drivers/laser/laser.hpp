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

#define GAIN_LT1789 (1+200/10)
#define PWM_MAX_VOLTAGE 1.8
#define PWM_MAX_VALUE 1024
#define MILLIAMPS_TO_AMPS 0.001

#define CURRENT_TO_VOLTAGE(current) \
    (current * MILLIAMPS_TO_AMPS * PWM_MAX_VALUE * GAIN_LT1789 / PWM_MAX_VOLTAGE)

constexpr float current_to_pwm = MILLIAMPS_TO_AMPS * PWM_MAX_VALUE * GAIN_LT1789 / PWM_MAX_VOLTAGE;
constexpr float pwm_to_current = 1 / current_to_pwm;

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

    uint32_t pwm_from_current(float current) {
        return uint32_t(current * current_to_pwm);
    }

    float current_from_pwm(uint32_t pwm) {
        return pwm * pwm_to_current;
    }

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

    float load_config() {
        uint32_t pwm = eeprom.read(EEPROM_CURRENT_ADDR);
        dvm.write32(config_map, PWM3_OFF, pwm);
        return MILLIAMPS_TO_AMPS * current_from_pwm(pwm);
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
