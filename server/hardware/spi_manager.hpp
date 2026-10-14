// SPI interface
// (c) Koheron
//
// https://www.kernel.org/doc/Documentation/spi/spidev

#ifndef __SERVER_CONTEXT_SPI_DEV_HPP__
#define __SERVER_CONTEXT_SPI_DEV_HPP__

#include "server/runtime/syslog.hpp"

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <array>
#include <unordered_map>
#include <span>
#include <string_view>
#include <climits>
#include <expected>
#include <system_error>

#include <linux/spi/spidev.h>

namespace hw {

class SpiDev
{
  public:
    using Result = std::expected<void, std::error_code>;
    using IoResult = std::expected<std::size_t, std::error_code>;

    explicit SpiDev(std::string devname_) : devname(std::move(devname_)) {}

    ~SpiDev();

    SpiDev(const SpiDev&) = delete;
    SpiDev& operator=(const SpiDev&) = delete;
    SpiDev(SpiDev&& other) noexcept {
        *this = std::move(other);
    }

    SpiDev& operator=(SpiDev&& other) noexcept;

    bool is_ok() const noexcept {
        return fd >= 0;
    }

    int init(uint8_t mode_, uint32_t speed_, uint8_t word_length_);
    int set_mode(uint8_t mode_);
    int set_full_mode(uint32_t mode32_);
    int set_speed(uint32_t speed_);
    int set_word_length(uint8_t word_length_);

    // Checked operations retain the error independently of errno, logging and
    // descriptor cleanup. The integer API remains available for existing drivers.
    [[nodiscard]] Result init_checked(uint8_t mode_, uint32_t speed_, uint8_t word_length_);
    [[nodiscard]] Result set_mode_checked(uint8_t mode_);
    [[nodiscard]] Result set_full_mode_checked(uint32_t mode32_);
    [[nodiscard]] Result set_speed_checked(uint32_t speed_);
    [[nodiscard]] Result set_word_length_checked(uint8_t word_length_);

    // -------- write

    template<typename T>
    int write(const T* buffer, uint32_t len) {
        const auto result = write_checked(buffer, len);
        // The legacy write API promises the complete byte count or failure.
        if (result && *result != size_t(len) * sizeof(T)) {
            return legacy_status(std::unexpected(std::make_error_code(std::errc::io_error)), "write");
        }
        return legacy_count(result, "write");
    }

    // Byte I/O reports the actual count, including short reads/writes and EOF.
    // A positive short transfer is never completed by another SPI transaction.
    [[nodiscard]] IoResult write_checked(std::span<const uint8_t> buffer);

    template<typename T>
    [[nodiscard]] IoResult write_checked(const T* buffer, uint32_t len) {
        // The byte count must fit both the kernel request and our int result.
        // Check before multiplying, including on 32-bit hosts.
        if (len > INT_MAX / sizeof(T)) {
            return std::unexpected(std::make_error_code(std::errc::value_too_large));
        }
        if (len != 0 && buffer == nullptr) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
        const uint8_t* p = reinterpret_cast<const uint8_t*>(buffer);
        return write_checked(std::span<const uint8_t>(p, size_t(len) * sizeof(T)));
    }

    // -------- recv

    int recv(std::span<uint8_t> buffer);
    int recv(uint8_t* buffer, size_t n_bytes);

    [[nodiscard]] IoResult recv_checked(std::span<uint8_t> buffer);
    [[nodiscard]] IoResult recv_checked(uint8_t* buffer, size_t n_bytes);

    template<typename T>
    int recv(std::vector<T>& vec) {
        return legacy_count(recv_checked(vec), "recv");
    }

    template<typename T>
    [[nodiscard]] IoResult recv_checked(std::vector<T>& vec) {
        if (vec.size() > INT_MAX / sizeof(T)) {
            return std::unexpected(std::make_error_code(std::errc::value_too_large));
        }
        return recv_checked(reinterpret_cast<uint8_t*>(vec.data()), vec.size() * sizeof(T));
    }

    template<typename T, size_t N>
    int recv(std::array<T, N>& arr) {
        return legacy_count(recv_checked(arr), "recv");
    }

    template<typename T, size_t N>
    [[nodiscard]] IoResult recv_checked(std::array<T, N>& arr) {
        if (N > INT_MAX / sizeof(T)) {
            return std::unexpected(std::make_error_code(std::errc::value_too_large));
        }
        return recv_checked(reinterpret_cast<uint8_t*>(arr.data()), N * sizeof(T));
    }

    // -------- transfer

    int transfer(std::span<const uint8_t> tx, std::span<uint8_t> rx);
    int transfer(uint8_t* tx_buff, uint8_t* rx_buff, size_t);

    // Success means the entire message completed. A short message is an I/O error.
    [[nodiscard]] Result transfer_checked(std::span<const uint8_t> tx, std::span<uint8_t> rx);
    [[nodiscard]] Result transfer_checked(uint8_t* tx_buff, uint8_t* rx_buff, size_t len);

    // TX+RX, same length N
    template<std::size_t N>
    int transfer(const std::array<uint8_t, N>& tx,
                 std::array<uint8_t, N>& rx) {
        return legacy_status(transfer_checked(tx, rx), "transfer");
    }

    template<std::size_t N>
    [[nodiscard]] Result transfer_checked(const std::array<uint8_t, N>& tx,
                                         std::array<uint8_t, N>& rx) {
        return transfer_checked(std::span<const uint8_t>(tx), std::span<uint8_t>(rx));
    }

    // Only transfers the first count bytes
    template<std::size_t Nt, std::size_t Nr>
    int transfer(const std::array<uint8_t, Nt>& tx,
                 std::array<uint8_t, Nr>& rx,
                 std::size_t count) {
        return legacy_status(transfer_checked(tx, rx, count), "transfer");
    }

    template<std::size_t Nt, std::size_t Nr>
    [[nodiscard]] Result transfer_checked(const std::array<uint8_t, Nt>& tx,
                                         std::array<uint8_t, Nr>& rx, std::size_t count) {
        if (count > Nt || count > Nr) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
        return transfer_checked(std::span<const uint8_t>(tx.data(), count),
                                std::span<uint8_t>(rx.data(), count));
    }

    template<std::size_t N>
    int transfer(const std::array<uint8_t, N>& tx, std::size_t count) {
        return legacy_status(transfer_checked(tx, count), "transfer");
    }

    template<std::size_t N>
    [[nodiscard]] Result transfer_checked(const std::array<uint8_t, N>& tx, std::size_t count) {
        if (count > N) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
        return transfer_checked(std::span<const uint8_t>(tx.data(), count), std::span<uint8_t>());
    }

    template<std::size_t N>
    int transfer(std::array<uint8_t, N>& rx, std::size_t count) {
        return legacy_status(transfer_checked(rx, count), "transfer");
    }

    template<std::size_t N>
    [[nodiscard]] Result transfer_checked(std::array<uint8_t, N>& rx, std::size_t count) {
        if (count > N) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
        return transfer_checked(std::span<const uint8_t>(), std::span<uint8_t>(rx.data(), count));
    }

    int fd_value() const noexcept { return fd; }
    std::string_view name() const noexcept { return devname; }

  private:
    std::string devname;

    uint8_t  mode = SPI_MODE_0;
    uint32_t mode32 = SPI_MODE_0;
    uint32_t speed = 1'000'000; // Hz
    uint8_t  word_length = 8;

    int fd = -1;

    int legacy_status(const Result& result, std::string_view operation) const;
    int legacy_count(const IoResult& result, std::string_view operation) const;
};

class SpiManager
{
  public:
    SpiManager();

    int init();

    bool has_device(std::string_view devname) const;

    SpiDev& get(std::string_view devname,
                uint8_t mode = SPI_MODE_0,
                uint32_t speed = 1'000'000,
                uint8_t word_length = 8);

  private:
    std::unordered_map<std::string, std::unique_ptr<SpiDev>> spi_drivers;
    SpiDev empty_spidev{""};
};

} // namespace hw

#endif // __SERVER_CONTEXT_SPI_DEV_HPP__
