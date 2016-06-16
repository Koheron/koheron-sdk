
#ifndef __DRIVERS_SPI_SPI_HPP__
#define __DRIVERS_SPI_SPI_HPP__

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/spi_dev.hpp>

class Spi
{
  public:
    Spi(Klib::DevMem& dvm_) {}

    void dummy(uint32_t u) {}
};

#endif // __DRIVERS_SPI_SPI_HPP__