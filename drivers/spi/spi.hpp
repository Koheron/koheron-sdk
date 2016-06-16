
#ifndef __DRIVERS_SPI_SPI_HPP__
#define __DRIVERS_SPI_SPI_HPP__

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/spi_dev.hpp>

class Spi
{
  public:
    Spi(Klib::DevMem& dvm_) {}

    int init(uint32_t mode) {
        spi_dev = SpiDev(mode);
        return spi_dev.init();
    }

  private:
    SpiDev spi_dev;
};

#endif // __DRIVERS_SPI_SPI_HPP__