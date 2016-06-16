#ifndef __DRIVERS_LIB_SPI_DEV_HPP__
#define __DRIVERS_LIB_SPI_DEV_HPP__

#include <cstdio>
#include <cstdint>
#include <unistd.h>

class SpiDev
{
  public:
    SpiDev(uint32_t mode_ = 0, uint32_t spi_speed_ = 1000000)
    : mode(mode_)
    , spi_speed(spi_speed_)
    {}

    ~SpiDev() {if (spi_fd >= 0) close(spi_fd);}

    int init();

  private:
    uint32_t mode;
    uint32_t spi_speed; // SPI bus speed

    int spi_fd = -1;
};

#endif // __DRIVERS_LIB_SPI_DEV_HPP__