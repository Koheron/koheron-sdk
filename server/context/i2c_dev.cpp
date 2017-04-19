// (c) Koheron

#include "i2c_dev.hpp"

#include <dirent.h>


// ---------------------------------------------------------------------
// I2cDev
// ---------------------------------------------------------------------

I2cDev::I2cDev(ContextBase& ctx_, std::string devname_)
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
            return -1;
        }
    }

    return 0;
}

// ---------------------------------------------------------------------
// I2cManager
// ---------------------------------------------------------------------

I2cManager::I2cManager(ContextBase& ctx_)
: ctx(ctx_)
, empty_i2cdev(std::make_unique<I2cDev>(ctx, ""))
{}

// Never return a negative number on failure.
// I2C missing is not considered critical as it might not
// be used by any driver.
int I2cManager::init()
{
    struct dirent *ent;
    DIR *dir = opendir("/sys/class/i2c-dev");

    if (dir == nullptr) {
        return 0;
    }

    while ((ent = readdir(dir)) != nullptr) {
        const char *devname = ent->d_name;

        // Exclude '.' and '..' repositories
        if (devname[0] != '.') {

            i2c_drivers.insert(
                std::make_pair(devname, std::make_unique<I2cDev>(ctx, devname))
            );
        }
    }

    closedir(dir);
    return 0;
}

bool I2cManager::has_device(const std::string& devname) const
{
    return i2c_drivers.find(devname) != i2c_drivers.end();
}
