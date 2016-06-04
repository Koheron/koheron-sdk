/// (c) Koheron

#include "laser.hpp"

Laser::Laser(Klib::DevMem& dvm_)
: dvm(dvm_),
  xadc(dvm_),
  gpio(dvm_),
  eeprom(dvm_)
{
    status = CLOSED;

    config_map = dvm.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);

    if (dvm.CheckMap(config_map) < 0 
        || xadc.Open() < 0 
        || gpio.Open() < 0)
        status = FAILED;
    
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

void Laser::set_laser_current(float current)
{
    float current_;
    current > MAX_LASER_CURRENT ? current_ = MAX_LASER_CURRENT : current_ = current;    
    uint32_t pwm = pwm_from_current(current);
    dvm.write32(config_map, PWM3_OFF, pwm);
}

