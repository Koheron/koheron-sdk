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

void Laser::set_laser_current(float current)
{
    float current_;
    current > MAX_LASER_CURRENT ? current_ = MAX_LASER_CURRENT : current_ = current;    
    uint32_t pwm = pwm_from_current(current);
    dvm.write32(config_map, PWM3_OFF, pwm);
}

