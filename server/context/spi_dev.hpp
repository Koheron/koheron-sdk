// SPI interface
// (c) Koheron
//
// From http://redpitaya.com/examples-new/spi/
// See also https://www.kernel.org/doc/Documentation/spi/spidev

#ifndef __DRIVERS_LIB_SPI_DEV_HPP__
#define __DRIVERS_LIB_SPI_DEV_HPP__

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <array>

#include <context_base.hpp>

// https://www.kernel.org/doc/Documentation/spi/spidev
class SpiDev
{
  public:
    SpiDev(ContextBase& ctx_, std::string devname_);

    ~SpiDev() {
        if (fd >= 0)
            close(fd);
    }

    bool is_ok() {return fd >= 0;}

    int init(uint8_t mode_, uint32_t speed_, uint8_t word_length_);
    int set_mode(uint8_t mode_);
    int set_full_mode(uint32_t mode32_);
    int set_speed(uint32_t speed_);

    /// Set the number of bits in each SPI transfer word.
    int set_word_length(uint8_t word_length_);

    template<typename T>
    int write(const T *buffer, uint32_t len)
    {
        if (fd >= 0)
            return ::write(fd, buffer, len * sizeof(T));

        return -1;
    }

    int recv(uint8_t *buffer, size_t n_bytes);
    int transfer(uint8_t *tx_buff, uint8_t *rx_buff, size_t len);

    template<typename T>
    int recv(std::vector<T>& vec) {
        return recv(reinterpret_cast<uint8_t*>(vec.data()),
                    vec.size() * sizeof(T));
    }

    template<typename T, size_t N>
    int recv(std::array<T, N>& arr) {
        return recv(reinterpret_cast<uint8_t*>(arr.data()),
                    N * sizeof(T));
    }

  private:
    ContextBase& ctx;
    std::string devname;

    uint8_t mode = SPI_MODE_0;
    uint32_t mode32 = SPI_MODE_0;
    uint32_t speed = 1000000; // SPI bus speed
    uint8_t word_length = 8;

    int fd = -1;
};

class SpiManager
{
  public:
    SpiManager(ContextBase& ctx_);

    int init();

    bool has_device(const std::string& devname);

    SpiDev& get(const std::string& devname,
                uint8_t mode = SPI_MODE_0,
                uint32_t speed = 1000000,
                uint8_t word_length = 8);

  private:
    ContextBase& ctx;
    std::unordered_map<std::string, std::unique_ptr<SpiDev>> spi_drivers;
    SpiDev empty_spidev;
};

#endif // __DRIVERS_LIB_SPI_DEV_HPP__
