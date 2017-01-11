/// Interface for remote I2C control and prototyping

#ifndef __DRIVERS_SPI_I2C_TESTS_I2C_HPP__
#define __DRIVERS_SPI_I2C_TESTS_I2C_HPP__

#include <context.hpp>

class I2c
{
  public:
    I2c(Context& ctx)
    : i2c(ctx.i2c.get("i2c-0"))
    {
        if (! i2c.is_ok()) {
            ctx.log<ERROR>("Cannot access i2c-0\n");
            return;
        }
    }

    int32_t write(int32_t addr, const std::vector<uint8_t>& buffer) {
        return i2c.write(addr, buffer.data(), buffer.size());
    }

  private:
    I2cDev& i2c;
};

#endif // __DRIVERS_SPI_I2C_TESTS_I2C_HPP__