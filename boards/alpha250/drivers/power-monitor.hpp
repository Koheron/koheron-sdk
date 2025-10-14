/// (c) Koheron

#ifndef __ALPHA_DRIVERS_POWER_MONITOR_HPP__
#define __ALPHA_DRIVERS_POWER_MONITOR_HPP__

#include <cstdint>
#include <array>

class Context;
namespace hw { class I2cDev; }

class PowerMonitor
{
  public:
    PowerMonitor(Context& ctx);

    std::array<float, 4> get_supplies_ui();
    float get_shunt_voltage(uint32_t index);
    float get_bus_voltage(uint32_t index);

  private:
    // index = 0: VCC main supply
    // index = 1: Clocking subsystem supply
    const std::array<uint8_t, 2> i2c_address = {{0b1000001, 0b1000101}};
    hw::I2cDev& i2c;

    // http://www.ti.com/lit/ds/symlink/ina230.pdf
    static constexpr uint8_t reg_configuration = 0;
    static constexpr uint8_t reg_shunt_voltage = 1;
    static constexpr uint8_t reg_bus_voltage = 2;
    static constexpr uint8_t reg_power = 3;
    static constexpr uint8_t reg_current = 4;
    static constexpr uint8_t reg_calibration = 5;
    static constexpr uint8_t reg_mask_enable = 6;
    static constexpr uint8_t reg_alert_limit = 7;
};

#endif // __ALPHA_DRIVERS_POWER_MONITOR_HPP__