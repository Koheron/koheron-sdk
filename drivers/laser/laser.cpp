/// (c) Koheron

#include "laser.hpp"

Laser::Laser(Klib::DevMem& dvm_)
: dvm(dvm_),
  xadc(dvm_),
  gpio(dvm_)
{
    status = CLOSED;

    config_map = dvm.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);

    if (dvm.CheckMap(config_map) < 0)
        status = FAILED;

    xadc.Open();
    gpio.Open();
    
    status = OPENED;
    reset();
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

