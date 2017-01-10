// SPI interface
// (c) Koheron
//
// From http://redpitaya.com/examples-new/spi/
// See also https://www.kernel.org/doc/Documentation/spi/spidev

#ifndef __DRIVERS_LIB_SPI_DEV_HPP__
#define __DRIVERS_LIB_SPI_DEV_HPP__

#include <cstdio>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

class Context;

class SpiDev
{
  public:
    SpiDev(Context& ctx_, uint32_t mode_ = 0, uint32_t speed_ = 1000000);

    ~SpiDev() {
        if (fd >= 0)
            close(fd);
    }

    int init();
    int set_mode(uint32_t mode_);
    int set_speed(uint32_t speed_);

    template<typename T>
    int write_buffer(const T *buffer, uint32_t len) {
        return write(fd, buffer, len * sizeof(T));
    }

  private:
    Context& ctx;

    uint32_t mode;
    uint32_t speed; // SPI bus speed

    int fd = -1;
};

#endif // __DRIVERS_LIB_SPI_DEV_HPP__
