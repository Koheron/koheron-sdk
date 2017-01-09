// I2C interface
// (c) Koheron

// See also https://www.kernel.org/doc/Documentation/spi/spidev

#ifndef __DRIVERS_LIB_I2C_DEV_HPP__
#define __DRIVERS_LIB_I2C_DEV_HPP__

#include <cstdio>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/i2c-dev.h>

class Context;

class I2cDev
{
  public:
    I2cDev(Context& ctx_);

    ~I2cDev() {
        if (fd >= 0)
            close(fd);
    }

    int init();

    int set_address(uint32_t addr);

    /// Write to I2C device
    /// addr: Addresse of the device to write to
    /// buffer: Pointer to the data to send
    /// len: Number of elements on the buffer array
    template<typename T>
    int write(uint32_t addr, const T *buffer, uint32_t len) {
        int err = set_address(addr);

        if (err < 0)
            return err;

        return ::write(fd, buffer, len * sizeof(T));
    }

  private:
    Context& ctx;
    int fd = -1;
};

#endif // __DRIVERS_LIB_I2C_DEV_HPP__