/// (c) Koheron

#include "laser.hpp"

Laser::Laser(Klib::DevMem& dvm_)
: dvm(dvm_),
  xadc(dvm_),
  gpio(dvm_),
  eeprom(dvm_)
{
    status = CLOSED;
}

Laser::~Laser()
{
    Close();
}

# define LASER_ENABLE_PIN 5

int Laser::Open()
{
    if (status == CLOSED) {
        // Config is required for the PWMs
        auto ids = dvm.RequestMemoryMaps<1>({{
            { CONFIG_ADDR, CONFIG_RANGE }
        }});

        if (dvm.CheckMapIDs(ids) < 0) {
            status = FAILED;
            return -1;
        }

        config_map = ids[0];
        
        // Open core drivers
        xadc.Open();
        gpio.Open();
        eeprom.Open();
        
        status = OPENED;
        reset();
    }
    
    return 0;
}

void Laser::Close()
{
    if (status == OPENED) {
        reset();
        status = CLOSED;
    }
}

void Laser::reset()
{
    assert(status == OPENED);
    xadc.set_channel(LASER_POWER_CHANNEL, LASER_CURRENT_CHANNEL);
    xadc.enable_averaging();
    xadc.set_averaging(256);
    gpio.set_as_output(LASER_ENABLE_PIN, 2);
    stop_laser();
    set_laser_current(0.0);
}

uint32_t Laser::get_laser_current()
{
    return xadc.read(LASER_CURRENT_CHANNEL);
}

uint32_t Laser::get_laser_power()
{
    return xadc.read(LASER_POWER_CHANNEL);
}

std::tuple<uint32_t, uint32_t> Laser::get_monitoring()
{
    return std::make_tuple(get_laser_current(), get_laser_power());
}

void Laser::start_laser()
{
    gpio.clear_bit(LASER_ENABLE_PIN, 2); // Laser enable on pin DIO7_P
}

void Laser::stop_laser()
{
    gpio.set_bit(LASER_ENABLE_PIN, 2); // Laser enable on pin DIO7_P
}

#define GAIN_LT1789 (1+200/10)
#define PWM_MAX_VOLTAGE 1.8
#define PWM_MAX_VALUE 1024
#define MILLIAMPS_TO_AMPS 0.001
#define CURRENT_TO_VOLTAGE(current) \
    (current * MILLIAMPS_TO_AMPS * PWM_MAX_VALUE * GAIN_LT1789 / PWM_MAX_VOLTAGE)

void Laser::set_laser_current(float current)
{
    float current_;
    current > MAX_LASER_CURRENT ? current_ = MAX_LASER_CURRENT : current_ = current;    
    uint32_t voltage = (uint32_t) CURRENT_TO_VOLTAGE(current_);
    dvm.write32(config_map, PWM3_OFF, voltage);
}

