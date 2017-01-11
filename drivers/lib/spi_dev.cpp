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
    if (fd >=0)
        return 0;

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
    if (! is_ok())
        return -1;

    mode = mode_;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        ctx.log<ERROR>("SpiDev [%s] Cannot set ioctl SPI_IOC_WR_MODE\n", devname);
        return -1;
    }

    return 0;
}

int SpiDev::set_speed(uint32_t speed_)
{
    if (! is_ok())
        return -1;

    speed = speed_;

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        ctx.log<ERROR>("SpiDev [%s] Cannot set ioctl SPI_IOC_WR_MAX_SPEED_HZ\n", devname);
        return -1;
    }

    return 0;
}

int SpiDev::recv(uint8_t *buffer, size_t n_bytes)
{
    if (! is_ok())
        return -1;

    int bytes_rcv = 0;
    uint64_t bytes_read = 0;

    while (bytes_read < n_bytes) {
        bytes_rcv = read(fd, buffer + bytes_read, n_bytes - bytes_read);

        if (bytes_rcv == 0) {
            ctx.log<INFO>("SpiDev [%s]: Connection to device closed\n", devname);
            return 0;
        }

        if (bytes_rcv < 0) {
            ctx.log<INFO>("SpiDev [%s]: Data reception failed\n", devname);
            return -1;
        }

        bytes_read += bytes_rcv;
    }

    assert(bytes_read == n_bytes);
    return bytes_read;
}

// ---------------------------------------------------------------------
// SpiManager
// ---------------------------------------------------------------------

SpiManager::SpiManager(Context& ctx_)
: ctx(ctx_)
, empty_spidev(ctx, "")
{}

// Never return a negative number on failure.
// SPI missing is not considered critical as it might not
// be used by any driver.
int SpiManager::init()
{
    struct dirent *ent;
    DIR *dir = opendir("/sys/class/spidev");

    if (dir == nullptr) {
        ctx.log<INFO>("SPI Manager: Cannot open /sys/class/spidev.\n");
        return 0;
    }

    while ((ent = readdir(dir)) != nullptr) {
        const char *devname = ent->d_name;

        // Exclude '.' and '..' repositories
        if (devname[0] != '.') {
            ctx.log<INFO>("SPI Manager: Found device '%s'\n", devname);

            spi_devices.insert(
                std::make_pair(devname, std::make_unique<SpiDev>(ctx, devname))
            );
        }
    }

    if (spi_devices.empty())
        ctx.log<INFO>("SPI Manager: No SPI device available.\n");

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
        // This is critical since explicit driver request cannot be honored
        ctx.log<CRITICAL>("SPI Manager: SPI device %s not found\n", devname.c_str());
        return empty_spidev;
    }

    spi_devices[devname]->init(mode, speed);
    return *spi_devices[devname];
}