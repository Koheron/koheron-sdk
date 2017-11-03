/// (c) Koheron

#ifndef __DRIVERS_TEMPERATURE_SENSOR_HPP__
#define __DRIVERS_TEMPERATURE_SENSOR_HPP__

#include <context.hpp>

// http://www.ti.com/lit/ds/symlink/tmp116.pdf

class TemperatureSensor
{
  public:
    TemperatureSensor(Context& ctx_)
    : ctx(ctx_)
    , i2c(ctx.i2c.get("i2c-0"))
    , xadc(ctx.mm.get<mem::xadc>())
    {}

    float get_temperature(uint32_t index) {
        // index = 0: temperature sensor near the voltage reference
        // index = 1: temperature sensor between RF ADC and PLL
        // index = 2: Zynq temperature using XADC
        if (index < 2) {
            uint16_t temp;
            if (i2c.write(i2c_address + index % 2, temperature_msb) < 0) {
                return 0;
            }
            if (i2c.read(i2c_address + index % 2, temp) < 0) {
                return 0;
            }
            temp = ((temp & 0xFF) << 8) + (temp >> 8);
            float temperature = ((temp - 32768) % 65536 + 32768) / 32768.0 * 256.0;
            return temperature;
        } else {
            return (xadc.read<0x200>() * 503.975) / 65356 - 273.15;
        }
    }

  private:
    static constexpr uint32_t i2c_address = 0b100'1000;
    Context& ctx;
    I2cDev& i2c;
    Memory<mem::xadc>& xadc;

    static constexpr uint8_t temperature_msb = 0;
    static constexpr uint8_t temperature_lsb = 1;
    static constexpr uint8_t status = 2;
    static constexpr uint8_t configuration = 3;

};

#endif // __DRIVERS_TEMPERATURE_SENSOR_HPP__