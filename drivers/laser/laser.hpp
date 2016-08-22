/// Laser development kit driver
///
/// (c) Koheron

#ifndef __DRIVERS_LASER_HPP__
#define __DRIVERS_LASER_HPP__

#include <tuple>

#include <drivers/lib/memory_manager.hpp>
#include <drivers/xadc/xadc.hpp>
#include <drivers/gpio/gpio.hpp>
#include <drivers/eeprom/eeprom.hpp>
#include <drivers/memory.hpp>

#include <thread>
#include <chrono>

// XADC channels
#define LASER_POWER_CHANNEL   1
#define LASER_CURRENT_CHANNEL 8

# define LASER_ENABLE_PIN 5 // Laser enable on pin DIO7_P

#define MAX_LASER_CURRENT 50.0 // mA

#define GAIN_LT1789 (1+200/10)
#define PWM_MAX_VOLTAGE 1.8
#define PWM_MAX_VALUE 1024
#define MILLIAMPS_TO_AMPS 0.001

constexpr float current_to_pwm = MILLIAMPS_TO_AMPS * PWM_MAX_VALUE * GAIN_LT1789 / PWM_MAX_VOLTAGE;
constexpr float pwm_to_current = 1 / current_to_pwm;

#define EEPROM_CURRENT_ADDR 0
#define TEST_EEPROM_ADDR 63

class Laser
{
  public:
    Laser(DevMem& dvm)
    : cfg(dvm.get<mem::config>())
    , xadc(dvm)
    , gpio(dvm)
    , eeprom(dvm)
    {
        reset();
    }

    void reset();

    uint32_t get_laser_current() {return xadc.read(LASER_CURRENT_CHANNEL);}
    uint32_t get_laser_power()   {return xadc.read(LASER_POWER_CHANNEL);}

    // TODO replace get_monitoring() with get_status()
    std::tuple<uint32_t, uint32_t> get_monitoring() {
        return std::make_tuple(get_laser_current(), get_laser_power());
    }

    std::tuple<bool, float, float> get_status() {
        float current = (0.0001/21.) * float(get_laser_current());
        float power = float(get_laser_power());
        return std::make_tuple(laser_on, current, power);
    }

    void start_laser() {gpio.clear_bit(LASER_ENABLE_PIN, 2); laser_on = true;}
    void stop_laser()  {gpio.set_bit(LASER_ENABLE_PIN, 2); laser_on = false;}

    void set_laser_current(float current);

    uint32_t pwm_from_current(float current) {return uint32_t(current * current_to_pwm);}
    float current_from_pwm(uint32_t pwm)     {return pwm * pwm_to_current;}

    bool is_laser_present() {
        eeprom.write_enable();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        eeprom.write(TEST_EEPROM_ADDR, 42);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return eeprom.read(TEST_EEPROM_ADDR) == 42;
    }

    void save_config() {
        uint32_t current = cfg.read<reg::pwm3>();
        eeprom.write(EEPROM_CURRENT_ADDR, current);
    }

    float load_config() {
        uint32_t pwm = eeprom.read(EEPROM_CURRENT_ADDR);
        cfg.write<reg::pwm3>(pwm);
        return MILLIAMPS_TO_AMPS * current_from_pwm(pwm);
    }

  private:
    MemoryMap<mem::config>& cfg; // required for pwm

    Xadc xadc;
    Gpio gpio;
    Eeprom eeprom;

    bool laser_on;
};

#endif // __DRIVERS_LASER_HPP__
