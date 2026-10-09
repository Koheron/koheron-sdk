/// (c) Koheron

#ifndef __ALPHA_DRIVERS_TEMPERATURE_SENSOR_HPP__
#define __ALPHA_DRIVERS_TEMPERATURE_SENSOR_HPP__

#include "server/runtime/services.hpp"
#include "server/hardware/i2c_manager.hpp"
#include "server/hardware/memory_manager.hpp"

#include <array>
#include <cstdint>

class TemperatureSensor
{
  public:
    // Temperatures in °C
    std::array<float, 3> get_temperatures() {
        return {
            get_tmp116_temperature(i2c_address_vref),
            get_tmp116_temperature(i2c_address_board),
            get_zynq_temperature()
        };
    }

    float get_zynq_temperature() {
        auto& xadc = hw::get_memory<mem::xadc>();
        // Average the 16-bit register codes exactly, without floating-point
        // drift or reading unfilled history. UG480 uses a 65536 full scale.
        zynq_sum -= zynq_codes[i];
        zynq_codes[i] = static_cast<uint16_t>(xadc.read<0x200>());
        zynq_sum += zynq_codes[i];
        if (++i == n_avg) { i = 0; }
        if (zynq_count < n_avg) { ++zynq_count; }
        const double mean_code = static_cast<double>(zynq_sum) / zynq_count;
        return static_cast<float>(mean_code * (503.975 / 65536.0) - 273.15);
    }

  private:
    static constexpr uint32_t i2c_address_vref = 0b1001000;  // Voltage reference sensor
    static constexpr uint32_t i2c_address_board = 0b1001001; // Board sensor

    static constexpr uint32_t n_avg = 100;
    uint32_t i = 0;
    uint32_t zynq_count = 0;
    uint32_t zynq_sum = 0; // At most 100 * 65535, safely within uint32_t.
    std::array<uint16_t, n_avg> zynq_codes{};

    // http://www.ti.com/lit/ds/symlink/tmp116.pdf
    static constexpr uint8_t temperature_msb = 0;
    static constexpr uint8_t temperature_lsb = 1;
    static constexpr uint8_t status = 2;
    static constexpr uint8_t configuration = 3;

    float get_tmp116_temperature(uint32_t i2c_address) {
        auto& i2c = services::require<hw::I2cManager>().get("i2c-0");

        uint16_t temp;

        if (i2c.write(i2c_address, temperature_msb) < 0) {
            return 0;
        }

        if (i2c.read(i2c_address, temp) < 0) {
            return 0;
        }

        temp = ((temp & 0xFF) << 8) + (temp >> 8);
        return (reinterpret_cast<int16_t&>(temp) * 0.0078125);
    }
};

#endif // __ALPHA_DRIVERS_TEMPERATURE_SENSOR_HPP__
