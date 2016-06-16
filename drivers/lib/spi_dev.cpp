
// From http://redpitaya.com/examples-new/spi/
// See also https://www.kernel.org/doc/Documentation/spi/spidev

#include "spi_dev.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

int SpiDev::init()
{
    spi_fd = open("/dev/spidev2.0", O_RDWR | O_NOCTTY);

    if (spi_fd < 0) {
        fprintf(stderr, "Cannot open /dev/spidev2.0\n");
        return -1;
    }

    /* Setting mode (CPHA, CPOL) */
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
        fprintf(stderr, "Cannot set ioctl SPI_IOC_WR_MODE\n");
        return -1;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed) < 0) {
        fprintf(stderr, "Cannot set ioctl SPI_IOC_WR_MAX_SPEED_HZ\n");
        return -1;
    }

    return 0;
}