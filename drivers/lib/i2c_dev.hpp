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

#include <unordered_map>
#include <string>
#include <memory>

class Context;

class I2cDev
{
  public:
    I2cDev(Context& ctx_, std::string devname_);

    ~I2cDev() {
        if (fd >= 0)
            close(fd);
    }

    bool is_ok() {return fd >= 0;}

    int init();

    int set_address(int32_t addr);

    /// Write to I2C device
    /// addr: Addresse of the device to write to
    /// buffer: Pointer to the data to send
    /// len: Number of elements on the buffer array
    template<typename T>
    int write(int32_t addr, const T *buffer, uint32_t len)
    {
        if (! is_ok())
            return -1;

        int err = set_address(addr);

        if (err < 0)
            return err;

        return ::write(fd, buffer, len * sizeof(T));
    }

    // TODO read

  private:
    Context& ctx;
    std::string devname;
    int fd = -1;
};

class I2cManager
{
  public:
    I2cManager(Context& ctx_);

    int init();

    bool has_device(const std::string& devname);

    I2cDev& get(const std::string& devname);

  private:
    Context& ctx;
    std::unordered_map<std::string, std::unique_ptr<I2cDev>> i2c_devices;
    I2cDev empty_i2cdev;
};

#endif // __DRIVERS_LIB_I2C_DEV_HPP__