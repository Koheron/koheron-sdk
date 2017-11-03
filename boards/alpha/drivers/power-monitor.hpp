/// (c) Koheron

#ifndef __DRIVERS_POWER_MONITOR_HPP__
#define __DRIVERS_POWER_MONITOR_HPP__

#include <context.hpp>

// http://www.ti.com/lit/ds/symlink/ina230.pdf

class PowerMonitor
{
  public:
    PowerMonitor(Context& ctx)
    : i2c(ctx.i2c.get("i2c-0"))
    {
        // Averages = 4. Conversion time = 8.244 ms
        std::array<uint8_t, 3> buff {reg_configuration, 0x43, 0xFF};
        i2c.write(i2c_address[0], buff);
        i2c.write(i2c_address[1], buff);
    }

    float get_shunt_voltage(uint32_t index) {
        uint16_t voltage;
        if (i2c.write(i2c_address[index], reg_shunt_voltage) < 0) {
            return -1.0;
        }
        if (i2c.read(i2c_address[index], voltage) < 0) {
            return -1.0;
        }
        voltage = ((voltage & 0xFF) << 8) + (voltage >> 8);
        return ((voltage - 32768) % 65536 + 32768) / 32768.0 * 81.9175 / 1000; // V
    }

    float get_bus_voltage(uint32_t index) {
        uint16_t voltage;
        if (i2c.write(i2c_address[index], reg_bus_voltage) < 0) {
            return -1.0;
        }
        if (i2c.read(i2c_address[index], voltage) < 0) {
            return -1.0;
        }
        voltage = ((voltage & 0xFF) << 8) + (voltage >> 8);
        return ((voltage - 32768) % 65536 + 32768) / 32768.0 *  40.95875; // V
    }

  private:
    const std::array<uint8_t, 2> i2c_address = {{0b100'0001, 0b100'0101}};
    I2cDev& i2c;

    static constexpr uint8_t reg_configuration = 0;
    static constexpr uint8_t reg_shunt_voltage = 1;
    static constexpr uint8_t reg_bus_voltage = 2;
    static constexpr uint8_t reg_power = 3;
    static constexpr uint8_t reg_current = 4;
    static constexpr uint8_t reg_calibration = 5;
    static constexpr uint8_t reg_mask_enable = 6;
    static constexpr uint8_t reg_alert_limit = 7;
};

#endif // __DRIVERS_POWER_MONITOR_HPP__