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
#include <array>
#include <atomic>
#include <thread>
#include <mutex>

#include <core/context_base.hpp>

class I2cDev
{
  public:
    I2cDev(ContextBase& ctx_, std::string devname_);

    ~I2cDev() {
        if (fd >= 0)
            close(fd);
    }

    bool is_ok() {return fd >= 0;}

    int init();

    /// Write to I2C device
    /// addr: Addresse of the device to write to
    /// buffer: Pointer to the data to send
    /// len: Number of elements on the buffer array
    template<int32_t addr, typename T>
    int write(const T *buffer, uint32_t len)
    {
        // Lock to avoid another process to change
        // the device address while writing
        std::lock_guard<std::mutex> lock(mutex);

        if (! is_ok())
            return -1;

        if (set_address<addr>() < 0)
            return -1;

        return ::write(fd, buffer, len * sizeof(T));
    }

    template<int32_t addr, typename T, size_t N>
    int write(const std::array<T, N>& buff) {
        return write<addr>(buff.data(), buff.size());
    }

    template<int32_t addr>
    int recv(uint8_t *buffer, size_t n_bytes)
    {
        // Lock to avoid another process to change
        // the device address while reading
        std::lock_guard<std::mutex> lock(mutex);

        if (! is_ok())
            return -1;

        if (set_address<addr>() < 0)
            return -1;

        int bytes_rcv = 0;
        uint64_t bytes_read = 0;

        while (bytes_read < n_bytes) {
            bytes_rcv = read(fd, buffer + bytes_read, n_bytes - bytes_read);

            if (bytes_rcv == 0) {
                ctx.log<INFO>("I2cDev [%s]: Connection to device closed\n", devname.c_str());
                return 0;
            }

            if (bytes_rcv < 0) {
                ctx.log<INFO>("I2cDev [%s]: Data reception failed\n", devname.c_str());
                return -1;
            }

            bytes_read += bytes_rcv;
        }

        assert(bytes_read == n_bytes);
        return bytes_read;
    }

  private:
    ContextBase& ctx;
    std::string devname;
    int fd = -1;
    std::atomic<int32_t> last_addr{-1};
    std::mutex mutex;

    template<int32_t addr>
    int set_address() {
        constexpr int32_t largest_i2c_addr = 127; // 7 bits long address
        static_assert(addr <= largest_i2c_addr, "Invalid I2C address");

        if (! is_ok())
            return -1;

        if (addr != last_addr) {
            if (ioctl(fd, I2C_SLAVE_FORCE, addr) < 0) {
                ctx.log<ERROR>("I2cDev [%s] Failed to acquire bus access and/or "
                               "talk to slave with address %i\n", devname.c_str(), addr);
                return -1;
            }

            ctx.log<INFO>("I2cDev [%s] Address set to %i\n", devname.c_str(), addr);
            last_addr = addr;
        }

        return 0;
    }
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
            ctx.log<CRITICAL>("I2C Manager: I2C device %s not found\n", devname.c_str());
            return *empty_i2cdev;
        }

        i2c_devices[devname]->init();
        return *i2c_devices[devname];
    }

  private:
    ContextBase& ctx;
    std::unordered_map<std::string, std::unique_ptr<I2cDev>> i2c_devices;
    std::unique_ptr<I2cDev> empty_i2cdev;
};

#endif // __DRIVERS_LIB_I2C_DEV_HPP__