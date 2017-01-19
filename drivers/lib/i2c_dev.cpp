// (c) Koheron

#include "i2c_dev.hpp"

#include <dirent.h>

#include <context.hpp>
#include <core/syslog.tpp> // FIXME Not very nice ...

// ---------------------------------------------------------------------
// I2cDev
// ---------------------------------------------------------------------

I2cDev::I2cDev(Context& ctx_, std::string devname_)
: ctx(ctx_)
, devname(devname_)
{}

int I2cDev::init()
{
    if (fd >=0)
        return 0;

    const char *devpath = ("/dev/" + devname).c_str();

    if (fd < 0) {
        fd = open(devpath, O_RDWR);

        if (fd < 0) {
            ctx.log<ERROR>("I2cDev: Cannot open %s\n", devpath);
            return -1;
        }
    }

    ctx.log<INFO>("I2cDev: %s opened\n", devpath);
    return 0;
}

int I2cDev::set_address(int32_t addr)
{
    if (! is_ok())
        return -1;

    if (addr != last_addr) {
        if (ioctl(fd, I2C_SLAVE_FORCE, addr) < 0) {
            ctx.log<ERROR>("I2cDev [%s] Failed to acquire bus access and/or "
                           "talk to slave with address %u.\n", devname.c_str(), addr);
            return -1;
        }

        last_addr = addr;
    }

    return 0;
}

int I2cDev::recv(int32_t addr, uint8_t *buffer, size_t n_bytes)
{
    if (! is_ok())
        return -1;

    if (set_address(addr) < 0)
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
            ctx.log<INFO>("I2cDevv [%s]: Data reception failed\n", devname.c_str());
            return -1;
        }

        bytes_read += bytes_rcv;
    }

    assert(bytes_read == n_bytes);
    return bytes_read;
}

// ---------------------------------------------------------------------
// I2cManager
// ---------------------------------------------------------------------

I2cManager::I2cManager(Context& ctx_)
: ctx(ctx_)
, empty_i2cdev(ctx, "")
{}

// Never return a negative number on failure.
// I2C missing is not considered critical as it might not
// be used by any driver.
int I2cManager::init()
{
    struct dirent *ent;
    DIR *dir = opendir("/sys/class/i2c-dev");

    if (dir == nullptr) {
        ctx.log<INFO>("I2C Manager: Cannot open /sys/class/i2c-dev.\n");
        return 0;
    }

    while ((ent = readdir(dir)) != nullptr) {
        const char *devname = ent->d_name;

        // Exclude '.' and '..' repositories
        if (devname[0] != '.') {
            ctx.log<INFO>("I2C Manager: Found device '%s'\n", devname);

            i2c_devices.insert(
                std::make_pair(devname, std::make_unique<I2cDev>(ctx, devname))
            );
        }
    }

    if (i2c_devices.empty())
        ctx.log<INFO>("I2C Manager: No I2C device available.\n");

    closedir(dir);
    return 0;
}

bool I2cManager::has_device(const std::string& devname)
{
    return i2c_devices.find(devname) != i2c_devices.end();
}

I2cDev& I2cManager::get(const std::string& devname)
{
    if (! has_device(devname)) {
        // This is critical since explicit driver request cannot be honored
        ctx.log<CRITICAL>("I2C Manager: I2C device %s not found\n", devname.c_str());
        return empty_i2cdev;
    }

    i2c_devices[devname]->init();
    return *i2c_devices[devname];
}