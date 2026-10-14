// (c) Koheron

#include "server/hardware/spi_manager.hpp"
#include "server/runtime/syslog.hpp"

#include <filesystem>
#include <cerrno>

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

int SpiDev::legacy_status(const Result& result, std::string_view operation) const {
    if (result) { return 0; }
    logf<ERROR>("SpiDev {}: {} failed: {}\n", devname, operation, result.error().message());
    errno = result.error().value();
    return -1;
}

int SpiDev::legacy_count(const IoResult& result, std::string_view operation) const {
    if (result) { return static_cast<int>(*result); }
    return legacy_status(std::unexpected(result.error()), operation);
}

int SpiDev::init(uint8_t mode_, uint32_t speed_, uint8_t word_length_) {
    return legacy_status(init_checked(mode_, speed_, word_length_), "init");
}

SpiDev::Result SpiDev::init_checked(uint8_t mode_, uint32_t speed_, uint8_t word_length_) {
    if (fd < 0) {
        const fs::path devpath = fs::path("/dev") / devname;
        // Use O_CLOEXEC to avoid fd leaks across exec
        fd = ::open(devpath.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);

        if (fd < 0) {
            return std::unexpected(std::error_code(errno, std::generic_category()));
        }
    }

    const auto result = set_mode_checked(mode_)
        .and_then([&] { return set_speed_checked(speed_); })
        .and_then([&] { return set_word_length_checked(word_length_); });
    if (!result) {
        // A partially configured device must not be advertised as ready.
        // Capture the error in result before close() can change errno.
        ::close(std::exchange(fd, -1));
        return result;
    }

    logf("SpiManager: Device {} initialized\n", devname);
    return {};
}

int SpiDev::set_mode(uint8_t mode_) {
    return legacy_status(set_mode_checked(mode_), "set_mode");
}

SpiDev::Result SpiDev::set_mode_checked(uint8_t mode_) {
    if (!is_ok()) {
        return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
    }

    if (::ioctl(fd, SPI_IOC_WR_MODE, &mode_) < 0) {
        return std::unexpected(std::error_code(errno, std::generic_category()));
    }

    mode = mode_;
    return {};
}

int SpiDev::set_full_mode(uint32_t mode32_) {
    return legacy_status(set_full_mode_checked(mode32_), "set_full_mode");
}

SpiDev::Result SpiDev::set_full_mode_checked(uint32_t mode32_) {
    if (!is_ok()) {
        return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
    }

    if (::ioctl(fd, SPI_IOC_WR_MODE32, &mode32_) < 0) {
        return std::unexpected(std::error_code(errno, std::generic_category()));
    }

    mode32 = mode32_;
    return {};
}

int SpiDev::set_speed(uint32_t speed_) {
    return legacy_status(set_speed_checked(speed_), "set_speed");
}

SpiDev::Result SpiDev::set_speed_checked(uint32_t speed_) {
    if (!is_ok()) {
        return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
    }

    if (::ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_) < 0) {
        return std::unexpected(std::error_code(errno, std::generic_category()));
    }

    speed = speed_;
    return {};
}

int SpiDev::set_word_length(uint8_t word_length_) {
    return legacy_status(set_word_length_checked(word_length_), "set_word_length");
}

SpiDev::Result SpiDev::set_word_length_checked(uint8_t word_length_) {
    if (!is_ok()) {
        return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
    }

    if (::ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &word_length_) < 0) {
        return std::unexpected(std::error_code(errno, std::generic_category()));
    }

    word_length = word_length_;
    return {};
}

int SpiDev::recv(std::span<uint8_t> buffer) {
    return legacy_count(recv_checked(buffer), "recv");
}

SpiDev::IoResult SpiDev::recv_checked(std::span<uint8_t> buffer) {
    if (!is_ok()) {
        return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
    }
    if (buffer.size() > INT_MAX) {
        return std::unexpected(std::make_error_code(std::errc::value_too_large));
    }
    if (buffer.empty()) { return 0; }

    ssize_t received;
    do {
        received = ::read(fd, buffer.data(), buffer.size());
    } while (received < 0 && errno == EINTR);
    if (received < 0) {
        return std::unexpected(std::error_code(errno, std::generic_category()));
    }
    return static_cast<size_t>(received);
}

int SpiDev::recv(uint8_t* buffer, size_t n_bytes) {
    return legacy_count(recv_checked(buffer, n_bytes), "recv");
}

SpiDev::IoResult SpiDev::recv_checked(uint8_t* buffer, size_t n_bytes) {
    if (n_bytes > INT_MAX) {
        return std::unexpected(std::make_error_code(std::errc::value_too_large));
    }
    if (n_bytes != 0 && buffer == nullptr) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    return recv_checked(std::span<uint8_t>(buffer, n_bytes));
}

int SpiDev::transfer(std::span<const uint8_t> tx, std::span<uint8_t> rx) {
    return legacy_status(transfer_checked(tx, rx), "transfer");
}

SpiDev::Result SpiDev::transfer_checked(std::span<const uint8_t> tx, std::span<uint8_t> rx) {
    if (!is_ok()) {
        return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
    }

    if (!tx.empty() && !rx.empty() && tx.size() != rx.size()) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    const size_t len = tx.empty() ? rx.size() : tx.size();
    // spidev reports message lengths as int. Do not truncate a size_t to u32.
    if (len > INT_MAX) {
        return std::unexpected(std::make_error_code(std::errc::value_too_large));
    }
    if (len == 0) { return {}; }

    spi_ioc_transfer tr{};
    // Empty spans may retain a non-null data pointer; the kernel uses null
    // pointers, not span lengths, to identify an unused transfer direction.
    tr.tx_buf = tx.empty() ? 0 : reinterpret_cast<__u64>(tx.data());
    tr.rx_buf = rx.empty() ? 0 : reinterpret_cast<__u64>(rx.data());
    tr.len    = static_cast<__u32>(len);
    tr.speed_hz      = speed;        // kernel uses current if 0; we set it
    tr.bits_per_word = word_length;  // likewise

    const auto transferred = ::ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (transferred < 0) {
        return std::unexpected(std::error_code(errno, std::generic_category()));
    }
    if (static_cast<size_t>(transferred) != len) {
        return std::unexpected(std::make_error_code(std::errc::io_error));
    }
    return {};
}

int SpiDev::transfer(uint8_t* tx_buff, uint8_t* rx_buff, size_t len) {
    return legacy_status(transfer_checked(tx_buff, rx_buff, len), "transfer");
}

SpiDev::Result SpiDev::transfer_checked(uint8_t* tx_buff, uint8_t* rx_buff, size_t len) {
    if (len > INT_MAX) {
        return std::unexpected(std::make_error_code(std::errc::value_too_large));
    }
    if (len != 0 && tx_buff == nullptr && rx_buff == nullptr) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    return transfer_checked(
        std::span<const uint8_t>(tx_buff, tx_buff ? len : 0),
        std::span<uint8_t>(rx_buff, rx_buff ? len : 0)
    );
}

SpiDev::IoResult SpiDev::write_checked(std::span<const uint8_t> buffer) {
    if (!is_ok()) {
        return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
    }
    if (buffer.size() > INT_MAX) {
        return std::unexpected(std::make_error_code(std::errc::value_too_large));
    }
    if (buffer.empty()) { return 0; }

    ssize_t written;
    do {
        written = ::write(fd, buffer.data(), buffer.size());
    } while (written < 0 && errno == EINTR);
    if (written < 0) {
        return std::unexpected(std::error_code(errno, std::generic_category()));
    }
    return static_cast<size_t>(written);
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
        logf<CRITICAL>("SpiManager: Device {} not found\n", devname);
        return empty_spidev;
    }

    if (const auto result = it->second->init_checked(mode, speed, word_length); !result) {
        logf<ERROR>("SpiManager: init({}) failed: {}\n", devname, result.error().message());
    }
    return *it->second;
}

} // namespace hw
