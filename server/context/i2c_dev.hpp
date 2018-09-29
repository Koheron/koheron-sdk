// I2C interface
// (c) Koheron

// See also https://www.kernel.org/doc/Documentation/spi/spidev

#ifndef __DRIVERS_LIB_I2C_DEV_HPP__
#define __DRIVERS_LIB_I2C_DEV_HPP__

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/i2c-dev.h>

#include <unordered_map>
#include <string>
#include <memory>
#include <array>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

#include <context_base.hpp>

class I2cDev
{
  public:
    I2cDev(ContextBase& ctx_, std::string devname_);

    ~I2cDev() {
        if (fd >= 0) {
            close(fd);
        }
    }

    bool is_ok() {return fd >= 0;}

    /// Write data to I2C driver

    /// addr: Address of the driver to write to
    /// buffer: Pointer to the data to send
    /// len: Number of elements on the buffer array
    int write(int32_t addr, const uint8_t *buffer, size_t n_bytes)
    {
        // Lock to avoid another process to change
        // the driver address while writing
        std::lock_guard<std::mutex> lock(mutex);

        if (! is_ok()) {
            return -1;
        }
        if (set_address(addr) < 0) {
            return -1;
        }

        return ::write(fd, buffer, n_bytes);
    }

    template<typename T>
    int write(int32_t addr, const T scalar) {
        return write(addr, &scalar, sizeof(T));
    }

    template<typename T, size_t N>
    int write(int32_t addr, const std::array<T, N>& buff) {
        return write(addr, buff.data(), buff.size() * sizeof(T));
    }

    template<typename T>
    int write(int32_t addr, const std::vector<T>& buff) {
        return write(addr, buff.data(), buff.size() * sizeof(T));
    }

    // Receive data from I2C driver
    int read(int32_t addr, uint8_t *buffer, size_t n_bytes)
    {
        // Lock to avoid another process to change
        // the driver address while reading
        std::lock_guard<std::mutex> lock(mutex);

        if (! is_ok()) {
            return -1;
        }

        if (set_address(addr) < 0) {
            return -1;
        }

        int bytes_rcv = 0;
        int64_t bytes_read = 0;

        while (bytes_read < int64_t(n_bytes)) {
            bytes_rcv = ::read(fd, buffer + bytes_read, n_bytes - bytes_read);

            if (bytes_rcv == 0) {
                return 0;
            }

            if (bytes_rcv < 0) {
                return -1;
            }

            bytes_read += bytes_rcv;
        }

        return bytes_read;
    }

    template<typename T>
    int read(int32_t addr, T& scalar) {
        return read(addr, reinterpret_cast<uint8_t*>(&scalar), sizeof(T));
    }

    template<typename T, size_t N>
    int read(int32_t addr, std::array<T, N>& data) {
        return read(addr, reinterpret_cast<uint8_t*>(data.data()), N * sizeof(T));
    }

    template<typename T>
    int read(int32_t addr, std::vector<T>& data) {
        return read(addr, reinterpret_cast<uint8_t*>(data.data()), data.size() * sizeof(T));
    }

  private:
    ContextBase& ctx;
    std::string devname;
    int fd = -1;
    std::atomic<int32_t> last_addr{-1};
    std::mutex mutex;

    int init();

    int set_address(int32_t addr) {
        // Check that address is 7 bits long
        if (addr < 0 || addr > 127) {
            return -1;
        }
        if (addr != last_addr) {
            if (ioctl(fd, I2C_SLAVE_FORCE, addr) < 0) {
                return -1;
            }
            last_addr = addr;
        }
        return 0;
    }

friend class I2cManager;
};

class I2cManager
{
  public:
    I2cManager(ContextBase& ctx_);

    int init();

    bool has_device(const std::string& devname) const;

    auto& get(const std::string& devname) {
        if (! has_device(devname)) {
            // This is critical since explicit driver request cannot be honored
            return *empty_i2cdev;
        }

        i2c_drivers[devname]->init();
        return *i2c_drivers[devname];
    }

  private:
    ContextBase& ctx;
    std::unordered_map<std::string, std::unique_ptr<I2cDev>> i2c_drivers;
    std::unique_ptr<I2cDev> empty_i2cdev;
};

#endif // __DRIVERS_LIB_I2C_DEV_HPP__
