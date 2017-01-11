// (c) Koheron

#include "spi_dev.hpp"

#include <dirent.h>

#include <context.hpp>
#include <core/syslog.tpp> // FIXME Not very nice ...

// ---------------------------------------------------------------------
// SpiDev
// ---------------------------------------------------------------------

SpiDev::SpiDev(Context& ctx_, std::string devname_)
: ctx(ctx_)
, devname(devname_)
{}

int SpiDev::init(uint32_t mode_, uint32_t speed_)
{
    const char *devpath = ("/dev/" + devname).c_str();

    if (fd < 0) {
        fd = open(devpath, O_RDWR | O_NOCTTY);

        if (fd < 0) {
            ctx.log<ERROR>("SpiDev: Cannot open %s\n", devpath);
            return -1;
        }
    }

    if (set_mode(mode_) < 0 || set_speed(speed_) < 0)
        return -1;

    ctx.log<INFO>("SpiDev: %s opened\n", devpath);
    return 0;
}

int SpiDev::set_mode(uint32_t mode_)
{
    mode = mode_;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        ctx.log<ERROR>("SpiDev [%s] Cannot set ioctl SPI_IOC_WR_MODE\n", devname);
        return -1;
    }

    return 0;
}

int SpiDev::set_speed(uint32_t speed_)
{
    speed = speed_;

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        ctx.log<ERROR>("SpiDev [%s] Cannot set ioctl SPI_IOC_WR_MAX_SPEED_HZ\n", devname);
        return -1;
    }

    return 0;
}

// ---------------------------------------------------------------------
// Spi
// ---------------------------------------------------------------------

SpiManager::SpiManager(Context& ctx_)
: ctx(ctx_)
, empty_spidev(ctx, "")
{}

int SpiManager::init()
{
    struct dirent *ent;
    DIR *dir = opendir("/sys/class/spidev");

    if (dir == nullptr) {
        ctx.log<INFO>("Spi Manager: Cannot open /sys/class/spidev.\n");
        return 0;
    }

    while ((ent = readdir(dir)) != nullptr) {
        const char *devname = ent->d_name;

        // Exclude '.' and '..' repositories
        if (devname[0] != '.') {
            ctx.log<INFO>("Spi Manager: Found device '%s'\n", devname);
            spi_devices.insert(std::make_pair(devname, std::make_unique<SpiDev>(ctx, devname)));
        }
    }

    if (spi_devices.empty())
        ctx.log<INFO>("Spi Manager: No SPI device available.\n");

    closedir(dir);
    return 0;
}

bool SpiManager::has_device(const std::string& devname)
{
    return spi_devices.find(devname) != spi_devices.end();
}

SpiDev& SpiManager::get(const std::string& devname, uint32_t mode, uint32_t speed)
{
    if (! has_device(devname)) {
        ctx.log<ERROR>("Spi Manager: Spi device %s not found\n", devname.c_str());
        return empty_spidev;
    }

    spi_devices[devname]->init(mode, speed);
    return *spi_devices[devname];
}