// I2C interface
// (c) Koheron

// See also https://www.kernel.org/doc/html/latest/i2c/dev-interface.html

#ifndef __SERVER_CONTEXT_I2C_DEV_HPP__
#define __SERVER_CONTEXT_I2C_DEV_HPP__

#include <unordered_map>
#include <string>
#include <memory>
#include <array>
#include <vector>
#include <mutex>
#include <expected>
#include <system_error>
#include <type_traits>

#include "server/runtime/syslog.hpp"

namespace hw {

class I2cDev
{
  public:
    I2cDev(std::string devname_);

    ~I2cDev();

    bool is_ok() {return fd >= 0;}

    /// Write data to I2C driver

    /// addr: Address of the driver to write to
    /// buffer: Pointer to the data to send
    /// n_bytes: Number of bytes in a single bus transaction.
    /// Returns n_bytes on completion or -1 on failure, including a short transfer.
    int write(int32_t addr, const uint8_t *buffer, size_t n_bytes);

    template<typename T> requires std::is_trivially_copyable_v<T>
    int write(int32_t addr, const T scalar) {
        return write(addr, reinterpret_cast<const uint8_t*>(&scalar), sizeof(T));
    }

    // Typed buffers retain their native byte representation, like read().
    template<typename T, size_t N> requires std::is_trivially_copyable_v<T>
    int write(int32_t addr, const std::array<T, N>& buff) {
        return write(addr, reinterpret_cast<const uint8_t*>(buff.data()), buff.size() * sizeof(T));
    }

    template<typename T> requires std::is_trivially_copyable_v<T>
    int write(int32_t addr, const std::vector<T>& buff) {
        return write(addr, reinterpret_cast<const uint8_t*>(buff.data()), buff.size() * sizeof(T));
    }

    // Receive one complete transaction; partial data must not be used on failure.
    int read(int32_t addr, uint8_t *buffer, size_t n_bytes);

    template<typename T> requires std::is_trivially_copyable_v<T>
    int read(int32_t addr, T& scalar) {
        return read(addr, reinterpret_cast<uint8_t*>(&scalar), sizeof(T));
    }

    template<typename T, size_t N> requires std::is_trivially_copyable_v<T>
    int read(int32_t addr, std::array<T, N>& data) {
        return read(addr, reinterpret_cast<uint8_t*>(data.data()), N * sizeof(T));
    }

    template<typename T> requires std::is_trivially_copyable_v<T>
    int read(int32_t addr, std::vector<T>& data) {
        return read(addr, reinterpret_cast<uint8_t*>(data.data()), data.size() * sizeof(T));
    }

  private:
    std::string devname;
    int fd = -1;
    // Accessed only by set_address() while read/write hold mutex.
    int32_t last_addr = -1;
    std::mutex mutex;

    std::expected<void, std::error_code> init();
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

} // namespace hw

#endif // __SERVER_CONTEXT_I2C_DEV_HPP__
