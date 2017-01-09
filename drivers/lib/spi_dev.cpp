
#include "spi_dev.hpp"

#include <context.hpp>

SpiDev::SpiDev(Context& ctx_, uint32_t mode_, uint32_t speed_)
: ctx(ctx_)
, mode(mode_)
, speed(speed_)
{}

int SpiDev::init() {
    if (fd < 0) {
        fd = open("/dev/spidev2.0", O_RDWR | O_NOCTTY);

        if (fd < 0) {
            ctx.log<ERROR>("Cannot open /dev/spidev2.0\n");
            return -1;
        }
    }

    if (set_mode(mode) < 0 || set_speed(speed) < 0)
        return -1;

    ctx.log<INFO>("/dev/spidev2.0 opened\n");
    return 0;
}

int SpiDev::set_mode(uint32_t mode_) {
    mode = mode_;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        ctx.log<ERROR>("Cannot set ioctl SPI_IOC_WR_MODE\n");
        return -1;
    }

    return 0;
}

int SpiDev::set_speed(uint32_t speed_) {
    speed = speed_;

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        ctx.log<ERROR>("Cannot set ioctl SPI_IOC_WR_MAX_SPEED_HZ\n");
        return -1;
    }

    return 0;
}