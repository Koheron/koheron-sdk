/// Interface for remote SPI control and prototyping

#ifndef __DRIVERS_SPI_SPI_HPP__
#define __DRIVERS_SPI_SPI_HPP__

#include <context.hpp>
#include <drivers/lib/spi_dev.hpp>

class Spi
{
  public:
    Spi(Context& ctx)
    : spi(ctx.spi.get("spidev2.0"))
    {
        if (! spi.is_ok()) {
            ctx.log<ERROR>("Cannot access spidev2.0\n");
            return;
        }

        spi.set_mode(SPI_MODE_0);
        spi.set_speed(1000000);
    }

    int32_t write(const std::vector<uint32_t>& buffer) {
        return spi.write(buffer.data(), buffer.size());
    }

    void set_speed(uint32_t speed) {
        spi.set_speed(speed);
    }

  private:
    SpiDev& spi;
};

#endif // __DRIVERS_SPI_SPI_HPP__
