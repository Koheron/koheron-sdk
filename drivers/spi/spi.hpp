/// Interface for remote SPI control and prototyping

#ifndef __DRIVERS_SPI_SPI_HPP__
#define __DRIVERS_SPI_SPI_HPP__

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/spi_dev.hpp>

class Spi
{
  public:
    Spi(DevMem& dvm_) {}

    int init(uint32_t mode) {
        spi_dev = SpiDev(mode);
        return spi_dev.init();
    }

    #pragma tcp-server write_array arg{buffer} arg{len}
    int write(const uint32_t *buffer, uint32_t len) {
        return spi_dev.write_buffer(buffer, len);
    }

  private:
    SpiDev spi_dev;
};

#endif // __DRIVERS_SPI_SPI_HPP__