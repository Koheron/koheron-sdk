// (c) Koheron

#include "server/context/i2c_dev.hpp"
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

int I2cDev::init() {
    if (fd >= 0) {
        return 0;
    }

    const char *devpath = ("/dev/" + devname).c_str();

    if (fd < 0) {
        fd = ::open(devpath, O_RDWR);

        if (fd < 0) {
            return -1;
        }
    }

    logf("I2cManager: Device {} initialized\n", devname);
    return 0;
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

    i2c_drivers[devname]->init();
    return *i2c_drivers[devname];
}