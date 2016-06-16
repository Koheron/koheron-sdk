
// From http://redpitaya.com/examples-new/spi/
// See also https://www.kernel.org/doc/Documentation/spi/spidev

#include "spi_dev.hpp"

int SpiDev::init()
{
    fd = open("/dev/spidev2.0", O_RDWR | O_NOCTTY);

    if (fd < 0) {
        fprintf(stderr, "Cannot open /dev/spidev2.0\n");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        fprintf(stderr, "Cannot set ioctl SPI_IOC_WR_MODE\n");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        fprintf(stderr, "Cannot set ioctl SPI_IOC_WR_MAX_SPEED_HZ\n");
        return -1;
    }

    return 0;
}
