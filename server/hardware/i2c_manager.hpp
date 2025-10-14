// I2C interface
// (c) Koheron

// See also https://www.kernel.org/doc/Documentation/spi/spidev

#ifndef __SERVER_CONTEXT_I2C_DEV_HPP__
#define __SERVER_CONTEXT_I2C_DEV_HPP__

#include <unordered_map>
#include <string>
#include <memory>
#include <array>
#include <vector>
#include <atomic>
#include <mutex>

#include "server/runtime/syslog.hpp"

class I2cDev
{
  public:
    I2cDev(std::string devname_);

    ~I2cDev();

    bool is_ok() {return fd >= 0;}

    /// Write data to I2C driver

    /// addr: Address of the driver to write to
    /// buffer: Pointer to the data to send
    /// len: Number of elements on the buffer array
    int write(int32_t addr, const uint8_t *buffer, size_t n_bytes);

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
    int read(int32_t addr, uint8_t *buffer, size_t n_bytes);

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
    std::string devname;
    int fd = -1;
    std::atomic<int32_t> last_addr{-1};
    std::mutex mutex;

    int init();
    int set_address(int32_t addr);

friend class I2cManager;
};

class I2cManager
{
  public:
    I2cManager();
    int init();
    bool has_device(const std::string& devname) const;
    I2cDev& get(const std::string& devname);

  private:
    std::unordered_map<std::string, std::unique_ptr<I2cDev>> i2c_drivers;
    std::unique_ptr<I2cDev> empty_i2cdev;
};

#endif // __SERVER_CONTEXT_I2C_DEV_HPP__
