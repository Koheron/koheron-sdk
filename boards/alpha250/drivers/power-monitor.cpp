#include "./power-monitor.hpp"

#include "server/context/context.hpp"

PowerMonitor::PowerMonitor(Context& ctx)
: i2c(ctx.i2c.get("i2c-0"))
{
    // Averages = 4. Conversion time = 8.244 ms
    std::array<uint8_t, 3> buff {reg_configuration, 0x43, 0xFF};
    i2c.write(i2c_address[0], buff);
    i2c.write(i2c_address[1], buff);
}

std::array<float, 4> PowerMonitor::get_supplies_ui() {
    return {
        100 * get_shunt_voltage(0), // VCC main current (A)
        get_bus_voltage(0),         // VCC main voltage (V)
        100 * get_shunt_voltage(1), // Clock current (A)
        get_bus_voltage(1)          // Clock voltage (V)
    };
}

float PowerMonitor::get_shunt_voltage(uint32_t index) {
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

float PowerMonitor::get_bus_voltage(uint32_t index) {
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