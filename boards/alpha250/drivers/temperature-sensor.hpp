/// (c) Koheron

#ifndef __ALPHA_DRIVERS_TEMPERATURE_SENSOR_HPP__
#define __ALPHA_DRIVERS_TEMPERATURE_SENSOR_HPP__

#include <context.hpp>

#include <array>

class TemperatureSensor
{
  public:
    TemperatureSensor(Context& ctx_)
    : ctx(ctx_)
    , i2c(ctx.i2c.get("i2c-0"))
    , xadc(ctx.mm.get<mem::xadc>())
    {
    }

    // Temperatures in °C
    std::array<float, 3> get_temperatures() {
        return {
            get_tmp116_temperature(i2c_address_vref),
            get_tmp116_temperature(i2c_address_board),
            get_zynq_temperature()
        };
    }

    float get_zynq_temperature() {
        return (xadc.read<0x200>() * 503.975) / 65356 - 273.15;
    }

  private:
    static constexpr uint32_t i2c_address_vref = 0b1001000;  // Voltage reference sensor
    static constexpr uint32_t i2c_address_board = 0b1001001; // Board sensor

    Context& ctx;
    I2cDev& i2c;
    Memory<mem::xadc>& xadc;

    // http://www.ti.com/lit/ds/symlink/tmp116.pdf
    static constexpr uint8_t temperature_msb = 0;
    static constexpr uint8_t temperature_lsb = 1;
    static constexpr uint8_t status = 2;
    static constexpr uint8_t configuration = 3;

    float get_tmp116_temperature(uint32_t i2c_address) {
        uint16_t temp;

        if (i2c.write(i2c_address, temperature_msb) < 0) {
            return 0;
        }

        if (i2c.read(i2c_address, temp) < 0) {
            return 0;
        }

        temp = ((temp & 0xFF) << 8) + (temp >> 8);
        return ((temp - 32768) % 65536 + 32768) / 32768.0 * 256.0;
    }
};

#endif // __ALPHA_DRIVERS_TEMPERATURE_SENSOR_HPP__