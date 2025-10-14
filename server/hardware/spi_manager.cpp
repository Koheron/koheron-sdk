// (c) Koheron

#include "server/hardware/spi_manager.hpp"
#include "server/runtime/syslog.hpp"

#include <filesystem>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>

namespace hw {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------
// SpiDev
// ---------------------------------------------------------------------

SpiDev::~SpiDev() {
    if (fd >= 0) {
        ::close(fd);
    }
}

SpiDev& SpiDev::operator=(SpiDev&& other) noexcept {
    if (this != &other) {
        if (fd >= 0) {
            ::close(fd);
        }

        fd = std::exchange(other.fd, -1);
        mode = other.mode;
        mode32 = other.mode32;
        speed = other.speed;
        word_length = other.word_length;
        devname = std::move(other.devname);
    }

    return *this;
}

int SpiDev::init(uint8_t mode_, uint32_t speed_, uint8_t word_length_) {
    if (fd < 0) {
        const fs::path devpath = fs::path("/dev") / devname;
        // Use O_CLOEXEC to avoid fd leaks across exec
        fd = ::open(devpath.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);

        if (fd < 0) {
            logf<WARNING>("SpiManager: open({}) failed: {}", devname, std::strerror(errno));
            return -1;
        }
    }

    if (set_mode(mode_) < 0 ||
        set_speed(speed_) < 0 ||
        set_word_length(word_length_) < 0) {
        return -1;
    }

    rt::print_fmt("SpiManager: Device {} initialized", devname);
    return 0;
}

int SpiDev::set_mode(uint8_t mode_) {
    if (!is_ok()) {
        return -1;
    }

    mode = mode_;

    if (::ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        logf<ERROR>("SPI_IOC_WR_MODE({}): {}", devname, std::strerror(errno));
        return -1;
    }

    return 0;
}

int SpiDev::set_full_mode(uint32_t mode32_) {
    if (!is_ok()) {
        return -1;
    }

    mode32 = mode32_;

    if (::ioctl(fd, SPI_IOC_WR_MODE32, &mode32) < 0) {
        logf<ERROR>("SPI_IOC_WR_MODE32({}): {}", devname, std::strerror(errno));
        return -1;
    }

    return 0;
}

int SpiDev::set_speed(uint32_t speed_) {
    if (!is_ok()) {
        return -1;
    }

    speed = speed_;

    if (::ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        logf<ERROR>("SPI_IOC_WR_MAX_SPEED_HZ({}): {}", devname, std::strerror(errno));
        return -1;
    }

    return 0;
}

int SpiDev::set_word_length(uint8_t word_length_) {
    if (!is_ok()) {
        return -1;
    }

    word_length = word_length_;

    if (::ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &word_length) < 0) {
        logf<ERROR>("SPI_IOC_WR_BITS_PER_WORD({}): {}", devname, std::strerror(errno));
        return -1;
    }

    return 0;
}

int SpiDev::recv(std::span<uint8_t> buffer) {
    if (!is_ok()) {
        return -1;
    }

    size_t total = 0;

    while (total < buffer.size()) {
        ssize_t r = ::read(fd, buffer.data() + total, buffer.size() - total);

        if (r > 0) {
            total += size_t(r);
            continue;
        }

        if (r == 0) {
            return int(total);      // short read (EOF-like)
        }

        if (errno == EINTR) {
            continue;
        }

        return -1;
    }

    return int(total);
}

int SpiDev::recv(uint8_t* buffer, size_t n_bytes) {
    return recv(std::span<uint8_t>(buffer, n_bytes));
}

int SpiDev::transfer(std::span<const uint8_t> tx, std::span<uint8_t> rx) {
    if (!is_ok()) {
        return -1;
    }

    if (!tx.empty() && !rx.empty() && tx.size() != rx.size()) {
        logf<ERROR>("SpiDev {}: tx/rx size mismatch ({} vs {})",
                    devname, tx.size(), rx.size());
        return -1;
    }

    const size_t len = tx.empty() ? rx.size() : tx.size();

    spi_ioc_transfer tr{};
    tr.tx_buf = reinterpret_cast<__u64>(tx.data());
    tr.rx_buf = reinterpret_cast<__u64>(rx.data());
    tr.len    = static_cast<__u32>(len);
    tr.speed_hz      = speed;        // kernel uses current if 0; we set it
    tr.bits_per_word = word_length;  // likewise

    return (::ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) ? -1 : 0;
}

int SpiDev::transfer(uint8_t* tx_buff, uint8_t* rx_buff, size_t len) {
    return transfer(
        std::span<const uint8_t>(tx_buff, tx_buff ? len : 0),
        std::span<uint8_t>(rx_buff, rx_buff ? len : 0)
    );
}

int SpiDev::write_u8(const uint8_t* p, uint32_t bytes) {
    if (fd < 0) {
        return -1;
    }

    size_t sent = 0;

    while (sent < bytes) {
        ssize_t w = ::write(fd, p + sent, bytes - sent);

        if (w > 0) {
            sent += size_t(w);
            continue;
        }

        if (w < 0 && errno == EINTR) {
            continue;
        }

        return -1;
    }

    return int(sent);
}

// ---------------------------------------------------------------------
// SpiManager
// ---------------------------------------------------------------------

SpiManager::SpiManager()
: empty_spidev("")
{}

int SpiManager::init() {
    const fs::path sys_spidev{"/sys/class/spidev"};
    std::error_code ec;

    if (!fs::exists(sys_spidev, ec) || !fs::is_directory(sys_spidev, ec)) {
        return 0; // not critical
    }

    for (const auto& entry : fs::directory_iterator(sys_spidev, ec)) {
        if (ec) {
            break;
        }

        const auto name = entry.path().filename().string();

        if (name.empty() || name[0] == '.') {
            continue;
        }

        logf("SpiManager: Found device {}\n", name);
        spi_drivers.emplace(name, std::make_unique<SpiDev>(name));
    }

    return 0;
}

bool SpiManager::has_device(std::string_view devname) const {
    return spi_drivers.find(std::string(devname)) != spi_drivers.end();
}

SpiDev& SpiManager::get(std::string_view devname,
                        uint8_t mode,
                        uint32_t speed,
                        uint8_t word_length) {
    auto it = spi_drivers.find(std::string(devname));

    if (it == spi_drivers.end()) {
        logf<CRITICAL>("SpiManager: Device {} not found", devname);
        return empty_spidev;
    }

    it->second->init(mode, speed, word_length);
    return *it->second;
}

} // namespace hw