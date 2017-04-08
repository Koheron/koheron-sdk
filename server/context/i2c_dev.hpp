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
        if (fd >= 0)
            close(fd);
    }

    bool is_ok() {return fd >= 0;}

    /// Write data to I2C driver

    /// addr: Addresse of the driver to write to
    /// buffer: Pointer to the data to send
    /// len: Number of elements on the buffer array
    template<int32_t addr, typename T>
    int write(const T *buffer, int32_t len)
    {
        // Lock to avoid another process to change
        // the driver address while writing
        std::lock_guard<std::mutex> lock(mutex);

        if (! is_ok())
            return -1;

        if (set_address<addr>() < 0)
            return -1;

        return ::write(fd, buffer, static_cast<uint32_t>(len) * sizeof(T));
    }

    template<int32_t addr, typename T, size_t N>
    int write(const std::array<T, N>& buff) {
        return write<addr>(buff.data(), buff.size());
    }

    template<int32_t addr, typename T>
    int write(const std::vector<T>& buff) {
        return write<addr>(buff.data(), buff.size());
    }

    // Receive data from I2C driver

    template<int32_t addr>
    int recv(uint8_t *buffer, size_t n_bytes)
    {
        // Lock to avoid another process to change
        // the driver address while reading
        std::lock_guard<std::mutex> lock(mutex);

        if (! is_ok())
            return -1;

        if (set_address<addr>() < 0)
            return -1;

        int bytes_rcv = 0;
        int64_t bytes_read = 0;

        while (bytes_read < n_bytes) {
            bytes_rcv = read(fd, buffer + bytes_read, n_bytes - bytes_read);

            if (bytes_rcv == 0) {
                return 0;
            }

            if (bytes_rcv < 0) {
                return -1;
            }

            bytes_read += bytes_rcv;
        }

        assert(bytes_read == n_bytes);
        return bytes_read;
    }

    template<int32_t addr, typename T, size_t N>
    int recv(std::array<T, N>& data) {
        return recv<addr>(reinterpret_cast<uint8_t*>(data.data()), N * sizeof(T));
    }

    template<int32_t addr, typename T>
    int recv(std::vector<T>& data) {
        return recv<addr>(reinterpret_cast<uint8_t*>(data.data()), data.size() * sizeof(T));
    }

  private:
    ContextBase& ctx;
    std::string devname;
    int fd = -1;
    std::atomic<int32_t> last_addr{-1};
    std::mutex mutex;

    int init();

    template<int32_t addr>
    int set_address() {
        constexpr int32_t largest_i2c_addr = 127; // 7 bits long address
        static_assert(addr <= largest_i2c_addr, "Invalid I2C address");

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