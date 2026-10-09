#include "./power-monitor.hpp"

#include "server/runtime/services.hpp"
#include "server/hardware/i2c_manager.hpp"
#include "server/utilities/endian_utils.hpp"

PowerMonitor::PowerMonitor()
: i2c(services::require<hw::I2cManager>().get("i2c-0"))
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
    if (index >= i2c_address.size()) { return -1.0f; }
    uint16_t voltage;
    if (i2c.write(i2c_address[index], reg_shunt_voltage) < 0) {
        return -1.0;
    }
    if (i2c.read(i2c_address[index], voltage) < 0) {
        return -1.0;
    }
    // INA230 shunt register: signed two's complement, 2.5 uV/LSB.
    const auto code = std::bit_cast<int16_t>(ut::from_be(voltage));
    return static_cast<float>(code) * 2.5e-6f; // V
}

float PowerMonitor::get_bus_voltage(uint32_t index) {
    if (index >= i2c_address.size()) { return -1.0f; }
    uint16_t voltage;
    if (i2c.write(i2c_address[index], reg_bus_voltage) < 0) {
        return -1.0;
    }
    if (i2c.read(i2c_address[index], voltage) < 0) {
        return -1.0;
    }
    // INA230 bus register: unsigned, 1.25 mV/LSB.
    return static_cast<float>(ut::from_be(voltage)) * 1.25e-3f; // V
}
