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

#include <unordered_map>
#include <string>
#include <memory>

class Context;

class SpiDev
{
  public:
    SpiDev(Context& ctx_, std::string devname_);

    ~SpiDev() {
        if (fd >= 0)
            close(fd);
    }

    bool is_ok() {return fd >= 0;}

    int init(uint32_t mode_, uint32_t speed_);
    int set_mode(uint32_t mode_);
    int set_speed(uint32_t speed_);

    template<typename T>
    int write(const T *buffer, uint32_t len)
    {
        if (fd >= 0)
            return ::write(fd, buffer, len * sizeof(T));

        return -1;
    }

    int recv(uint8_t *buffer, size_t n_bytes);

    // TODO read

  private:
    Context& ctx;
    std::string devname;

    uint32_t mode = SPI_MODE_0;
    uint32_t speed = 1000000; // SPI bus speed

    int fd = -1;
};

class SpiManager
{
  public:
    SpiManager(Context& ctx_);

    int init();

    bool has_device(const std::string& devname);

    SpiDev& get(const std::string& devname,
                uint32_t mode = SPI_MODE_0,
                uint32_t speed = 1000000);

  private:
    Context& ctx;
    std::unordered_map<std::string, std::unique_ptr<SpiDev>> spi_devices;
    SpiDev empty_spidev;
};

#endif // __DRIVERS_LIB_SPI_DEV_HPP__
