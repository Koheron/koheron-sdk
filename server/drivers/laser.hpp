/// Laser controller
///
/// (c) Koheron

#ifndef __DRIVERS_LASER_CONTROLLER_HPP__
#define __DRIVERS_LASER_CONTROLLER_HPP__

#include <context.hpp>
#include <xadc.hpp>

namespace Laser_params {
    constexpr uint32_t power_channel = 1; //xadc channel
    constexpr uint32_t current_channel = 8; //xadc channel
    constexpr float max_laser_current = 50.0; // mA
    constexpr float gain_lt1789 = 1+200./10;
    constexpr float pwm_max_voltage = 1.8; // V
    constexpr float pwm_max_value = (1 << prm::pwm_width);
    constexpr float milliamps_to_amps = 0.001;
    constexpr float current_to_pwm = milliamps_to_amps * pwm_max_value * gain_lt1789 / pwm_max_voltage;
    constexpr float measured_current_gain = 0.1F * milliamps_to_amps / gain_lt1789;
}

class Laser
{
  public:
    Laser(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , xadc(ctx.get<Xadc>())
    {
        stop();
        current = 0;
        set_current(current);
    }

    void start() {
        ctl.clear_bit<reg::laser_control, 0>();
        laser_on = true;
    }

    void stop() {
        ctl.set_bit<reg::laser_control, 0>();
        laser_on = false;
    }

    void set_current(float current_value) {
        if (constant_power_on == true) {
            // Switch to constant current mode
            ctl.clear_bit<reg::laser_control, 2>();
            constant_power_on = false;
        }
        current = std::min(current_value, Laser_params::max_laser_current);
        ctl.write<reg::laser_current>(uint32_t(current * Laser_params::current_to_pwm));
    }

    void set_power(float power_value) {
        if (constant_power_on == false) {
            // Switch to constant power_mode
            ctl.set_bit<reg::laser_control, 2>();
            constant_power_on = true;
        }
        power = power_value;
        ctl.write<reg::power_setpoint>(uint32_t(power));
    }

    float get_measured_current() {
        return Laser_params::measured_current_gain * float(xadc.read(Laser_params::current_channel));
    }

    float get_measured_power() {
        return xadc.read(Laser_params::power_channel);
    }

    void switch_mode() {
        constant_power_on = !constant_power_on;
        ctl.write_bit<reg::laser_control, 2>(constant_power_on);
    }

    auto get_status() {
        float measured_current = get_measured_current();
        float measured_power = get_measured_power();
        return std::make_tuple(
            laser_on,
            constant_power_on,
            current,
            power,
            measured_current,
            measured_power
        );
    }

  private:
    float current;
    float power;
    bool laser_on;
    bool constant_power_on;
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Xadc& xadc;

};

#endif // __DRIVERS_LASER_HPP__