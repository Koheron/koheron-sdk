/// Laser controller
///
/// (c) Koheron

#ifndef __DRIVERS_LASER_CONTROLLER_HPP__
#define __DRIVERS_LASER_CONTROLLER_HPP__

#include <context.hpp>
#include <xadc.hpp>
#include <eeprom.hpp>
#include <chrono>

namespace Laser_params {
    constexpr uint32_t power_channel = 1; //xadc channel
    constexpr uint32_t current_channel = 8; //xadc channel
    constexpr float max_laser_current = 50.0; // mA
    constexpr float current_gain = 47.7; // mA/V
    constexpr float pwm_max_voltage = 1.8; // V
    constexpr float pwm_max_value = (1 << prm::pwm_width);
    constexpr float current_to_pwm = pwm_max_value / ( current_gain * pwm_max_voltage);
    constexpr float measured_current_gain = current_gain * 1E-7F;
}

class Laser
{
  public:
    Laser(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , xadc(ctx.get<Xadc>())
    , eeprom(ctx.get<Eeprom>())
    {
        stop();
        current = 0;
        set_current(current);
        read_calibration();
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
        ctl.write<reg::power_setpoint>(uint32_t((power_1mW - power_0mW) / 1000.0F * power_value + power_0mW));
    }

    float get_measured_current() {
        return Laser_params::measured_current_gain * float(xadc.read(Laser_params::current_channel));
    }

    float get_measured_power() {
        return 1000 * (float(xadc.read(Laser_params::power_channel)) - power_0mW) / (power_1mW - power_0mW);
    }

    void switch_mode() {
        if (!constant_power_on) {
            if (laser_on) {
                // integral reset
                stop();
                start();
            }
            set_power(get_measured_power());
        } else {
            set_current(sts.read<reg::pid_control>() / Laser_params::current_to_pwm);
        }
    }

    auto get_status() {
        float measured_current = get_measured_current();
        float measured_power = get_measured_power();
        return std::make_tuple(
            laser_on,
            constant_power_on,
            is_calibrated,
            current,
            power,
            measured_current,
            measured_power
        );
    }

    void read_calibration() {
        if (eeprom.read(0) == 1) {
            is_calibrated = true;
            power_0mW = eeprom.read(1);
            power_1mW = eeprom.read(2);
        } else {
            is_calibrated = false;
            power_0mW = 300;
            power_1mW = 1850;
        }
    }

    void calibrate_0mW() {
        using namespace std::chrono_literals;
        stop();
        std::this_thread::sleep_for(200ms);
        uint32_t sum = 0;
        for (auto i = 0; i<100; i++) {
            sum += xadc.read(Laser_params::power_channel);
        }
        eeprom.write(1, sum/100);
        std::this_thread::sleep_for(5ms);
        ctx.log<INFO>("Power 0mW = %u\n", eeprom.read(1));
        set_current(15.0);
        start();
    }

    void calibrate_1mW() {
        using namespace std::chrono_literals;
        uint32_t sum = 0;
        for (auto i = 0; i<100; i++) {
            sum += xadc.read(Laser_params::power_channel);
        }
        eeprom.write(2, sum/100);
        std::this_thread::sleep_for(5ms);
        eeprom.write(0, 1);
        std::this_thread::sleep_for(5ms);
        ctx.log<INFO>("Power 1mW = %u\n", eeprom.read(2));
        read_calibration();
    }

  private:
    float current;
    float power;
    bool laser_on;
    bool constant_power_on;
    bool is_calibrated;
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Xadc& xadc;

    // Calibration
    Eeprom& eeprom;
    uint32_t power_0mW;
    uint32_t power_1mW;

};

#endif // __DRIVERS_LASER_HPP__