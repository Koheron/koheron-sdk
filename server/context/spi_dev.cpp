// (c) Koheron

#include "spi_dev.hpp"
#include <server/core/lib/syslog.hpp>

#include <dirent.h>

// ---------------------------------------------------------------------
// SpiDev
// ---------------------------------------------------------------------

SpiDev::SpiDev(std::string devname_)
: devname(devname_)
{}

int SpiDev::init(uint8_t mode_, uint32_t speed_, uint8_t word_length_) {
    if (fd >= 0) {
        return 0;
    }

    const char *devpath = ("/dev/" + devname).c_str();

    if (fd < 0) {
        fd = open(devpath, O_RDWR | O_NOCTTY);

        if (fd < 0) {
            return -1;
        }
    }

    if (set_mode(mode_) < 0              ||
        set_speed(speed_) < 0            ||
        set_word_length(word_length_) < 0) {
        return -1;
    }

    koheron::print_fmt<INFO>("SpiManager: Device {} initialized", devname);
    return 0;
}

int SpiDev::set_mode(uint8_t mode_) {
    if (! is_ok()) {
        return -1;
    }

    mode = mode_;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        return -1;
    }

    return 0;
}

int SpiDev::set_full_mode(uint32_t mode32_) {
    if (! is_ok()) {
        return -1;
    }

    mode32 = mode32_;

    if (ioctl(fd, SPI_IOC_WR_MODE32, &mode32) < 0) {
        return -1;
    }

    return 0;
}

int SpiDev::set_speed(uint32_t speed_) {
    if (! is_ok()) {
        return -1;
    }

    speed = speed_;

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        return -1;
    }

    return 0;
}

int SpiDev::set_word_length(uint8_t word_length_) {
    if (! is_ok()) {
        return -1;
    }

    word_length = word_length_;

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &word_length) < 0) {
        return -1;
    }

    return 0;
}

int SpiDev::recv(uint8_t *buffer, size_t n_bytes) {
    if (! is_ok()) {
        return -1;
    }

    int bytes_rcv = 0;
    size_t bytes_read = 0;

    while (bytes_read < n_bytes) {
        bytes_rcv = read(fd, buffer + bytes_read, n_bytes - bytes_read);

        if (bytes_rcv == 0) {
            return 0;
        }

        if (bytes_rcv < 0) {
            return -1;
        }

        bytes_read += bytes_rcv;
    }

    assert(bytes_read == n_bytes);
    return bytes_read;
}

int SpiDev::transfer(uint8_t *tx_buff, uint8_t *rx_buff, size_t len) {
    if (! is_ok()) {
        return -1;
    }

    struct spi_ioc_transfer tr{};
    tr.tx_buf = reinterpret_cast<unsigned long>(tx_buff);
    tr.rx_buf = reinterpret_cast<unsigned long>(rx_buff);
    tr.len = len;
    return ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

// ---------------------------------------------------------------------
// SpiManager
// ---------------------------------------------------------------------

SpiManager::SpiManager()
: empty_spidev("")
{}

// Never return a negative number on failure.
// SPI missing is not considered critical as it might not
// be used by any driver.
int SpiManager::init() {
    struct dirent *ent;
    DIR *dir = opendir("/sys/class/spidev");

    if (dir == nullptr) {
        return 0;
    }

    while ((ent = readdir(dir)) != nullptr) {
        const char *devname = ent->d_name;

        // Exclude '.' and '..' repositories
        if (devname[0] != '.') {
            koheron::print_fmt<INFO>("SpiManager: Found device {}", devname);

            spi_drivers.insert(
                std::make_pair(devname, std::make_unique<SpiDev>(devname))
            );
        }
    }

    closedir(dir);
    return 0;
}

bool SpiManager::has_device(const std::string& devname) {
    return spi_drivers.find(devname) != spi_drivers.end();
}

SpiDev& SpiManager::get(const std::string& devname,
                        uint8_t mode, uint32_t speed, uint8_t word_length) {
    if (! has_device(devname)) {
        koheron::print_fmt<CRITICAL>("SpiManager: Device {} not found", devname);
        return empty_spidev;
    }

    spi_drivers[devname]->init(mode, speed, word_length);
    return *spi_drivers[devname];
}
