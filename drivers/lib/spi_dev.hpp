#ifndef __DRIVERS_LIB_SPI_DEV_HPP__
#define __DRIVERS_LIB_SPI_DEV_HPP__

#include <cstdio>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

class SpiDev
{
  public:
    SpiDev(uint32_t mode_ = 0, uint32_t speed_ = 1000000)
    : mode(mode_)
    , speed(speed_)
    {}

    ~SpiDev() {if (fd >= 0) close(fd);}

    int init();

    int write_buffer(const uint32_t *buffer, uint32_t len) {return write(fd, buffer, len * sizeof(uint32_t));}

  private:
    uint32_t mode;
    uint32_t speed; // SPI bus speed

    int fd = -1;
};

#endif // __DRIVERS_LIB_SPI_DEV_HPP__