/// Laser controller
///
/// (c) Koheron

#ifndef __DRIVERS_LASER_CONTROLLER_HPP__
#define __DRIVERS_LASER_CONTROLLER_HPP__

#include <context.hpp>
#include <drivers/xadc/xadc.hpp>

// XADC channels
#define LASER_POWER_CHANNEL   1
#define LASER_CURRENT_CHANNEL 8

// Laser current
#define MAX_LASER_CURRENT 50.0 // mA
#define GAIN_LT1789 (1+200/10)
#define PWM_MAX_VOLTAGE 1.8
#define PWM_MAX_VALUE (1 << prm::pwm_width)
#define MILLIAMPS_TO_AMPS 0.001
constexpr float current_to_pwm = MILLIAMPS_TO_AMPS * PWM_MAX_VALUE * GAIN_LT1789 / PWM_MAX_VOLTAGE;


class Laser
{
  public:
    Laser(Context& ctx)
    : cfg(ctx.mm.get<mem::config>())
    , sts(ctx.mm.get<mem::status>())
    , xadc(ctx.get<Xadc>())
    {}

    void start() {
        cfg.write<reg::laser_shutdown>(0);
    }

    void stop() {
        cfg.write<reg::laser_shutdown>(1);
    }

    void set_current(float current) {
        // Current in mA
        float current_;
        current > MAX_LASER_CURRENT ? current_ = MAX_LASER_CURRENT : current_ = current;
        cfg.write<reg::laser_current>(uint32_t(current_ * current_to_pwm));
    }

    uint32_t get_current() {
        return xadc.read(LASER_CURRENT_CHANNEL);
    }

    uint32_t get_power() {
        return xadc.read(LASER_POWER_CHANNEL);
    }

  private:
    Memory<mem::config>& cfg;
    Memory<mem::status>& sts;
    Xadc& xadc;

};

#endif // __DRIVERS_LASER_HPP__
