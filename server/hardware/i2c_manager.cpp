// (c) Koheron

#include "server/hardware/i2c_manager.hpp"
#include "server/runtime/syslog.hpp"

#include <cstdio>
#include <cstdint>
#include <memory>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/i2c-dev.h>
#include <dirent.h>

namespace hw {

// ---------------------------------------------------------------------
// I2cDev
// ---------------------------------------------------------------------

I2cDev::I2cDev(std::string devname_)
: devname(devname_)
{}

I2cDev::~I2cDev() {
    if (fd >= 0) {
        ::close(fd);
    }
}

std::expected<void, std::error_code> I2cDev::init() {
    if (fd >= 0) {
        return {};
    }

    const std::string devpath = "/dev/" + devname;
    fd = ::open(devpath.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return std::unexpected(std::error_code(errno, std::generic_category()));
    }

    logf("I2cManager: Device {} initialized\n", devname);
    return {};
}

int I2cDev::write(int32_t addr, const uint8_t *buffer, size_t n_bytes) {
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

int I2cDev::read(int32_t addr, uint8_t *buffer, size_t n_bytes) {
    // Lock to avoid another process to change
    // the driver address while reading
    std::lock_guard lock(mutex);

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

int I2cDev::set_address(int32_t addr) {
    // Check that address is 7 bits long
    if (addr < 0 || addr > 127) {
        return -1;
    }

    if (addr != last_addr) {
        if (::ioctl(fd, I2C_SLAVE_FORCE, addr) < 0) {
            return -1;
        }

        last_addr = addr;
    }

    return 0;
}

// ---------------------------------------------------------------------
// I2cManager
// ---------------------------------------------------------------------

I2cManager::I2cManager()
: empty_i2cdev(std::make_unique<I2cDev>(""))
{}

// Never return a negative number on failure.
// I2C missing is not considered critical as it might not
// be used by any driver.
int I2cManager::init() {
    struct dirent *ent;
    DIR *dir = opendir("/sys/class/i2c-dev");

    if (dir == nullptr) {
        return 0;
    }

    while ((ent = readdir(dir)) != nullptr) {
        const char *devname = ent->d_name;

        // Exclude '.' and '..' repositories
        if (devname[0] != '.') {
            logf("I2cManager: Found device {}\n", devname);

            i2c_drivers.insert(
                std::make_pair(devname, std::make_unique<I2cDev>(devname))
            );
        }
    }

    closedir(dir);
    return 0;
}

bool I2cManager::has_device(const std::string& devname) const {
    return i2c_drivers.find(devname) != i2c_drivers.end();
}

I2cDev& I2cManager::get(const std::string& devname) {
    if (! has_device(devname)) {
        logf<CRITICAL>("I2cManager: Device {} not found\n", devname);
        return *empty_i2cdev;
    }

    auto& device = *i2c_drivers[devname];
    if (const auto result = device.init(); !result) {
        logf<ERROR>("I2cManager: open({}) failed: {}\n", devname, result.error().message());
    }
    return device;
}

} // namespace hw
