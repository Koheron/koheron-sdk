// SPI interface
// (c) Koheron
//
// https://www.kernel.org/doc/Documentation/spi/spidev

#ifndef __DRIVERS_LIB_SPI_DEV_HPP__
#define __DRIVERS_LIB_SPI_DEV_HPP__

#include "server/runtime/syslog.hpp"

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <array>
#include <unordered_map>
#include <span>
#include <string_view>
#include <cassert>

#include <linux/spi/spidev.h>

class SpiDev
{
  public:
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

    // -------- write

    template<typename T>
    int write(const T* buffer, uint32_t len) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(buffer);
        return write_u8(p, size_t(len) * sizeof(T));
    }

    // -------- recv

    int recv(std::span<uint8_t> buffer);
    int recv(uint8_t* buffer, size_t n_bytes);

    template<typename T>
    int recv(std::vector<T>& vec) {
        return recv(std::span<uint8_t>(reinterpret_cast<uint8_t*>(vec.data()),
                                       vec.size() * sizeof(T)));
    }

    template<typename T, size_t N>
    int recv(std::array<T, N>& arr) {
        return recv(std::span<uint8_t>(reinterpret_cast<uint8_t*>(arr.data()),
                                       N * sizeof(T)));
    }

    // -------- transfer

    int transfer(std::span<const uint8_t> tx, std::span<uint8_t> rx);
    int transfer(uint8_t* tx_buff, uint8_t* rx_buff, size_t);

    // TX+RX, same length N
    template<std::size_t N>
    int transfer(const std::array<uint8_t, N>& tx,
                 std::array<uint8_t, N>& rx) {
        return transfer(std::span<const uint8_t>(tx),
                        std::span<uint8_t>(rx));
    }

    // Only transfers the first count bytes
    template<std::size_t Nt, std::size_t Nr>
    int transfer(const std::array<uint8_t, Nt>& tx,
                 std::array<uint8_t, Nr>& rx,
                 std::size_t count) {
        assert(count <= Nt && "count exceeds tx size");
        assert(count <= Nr && "count exceeds rx size");
        return transfer(std::span<const uint8_t>(tx.data(), count),
                        std::span<uint8_t>(rx.data(), count));
    }

    template<std::size_t N>
    int transfer(const std::array<uint8_t, N>& tx, std::size_t count) {
        assert(count <= N && "count exceeds tx size");
        return transfer(std::span<const uint8_t>(tx.data(), count),
                        std::span<uint8_t>());
    }

    template<std::size_t N>
    int transfer(std::array<uint8_t, N>& rx, std::size_t count) {
        assert(count <= N && "count exceeds rx size");
        return transfer(std::span<const uint8_t>(),
                        std::span<uint8_t>(rx.data(), count));
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

    int write_u8(const uint8_t* p, uint32_t bytes);
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

#endif // __DRIVERS_LIB_SPI_DEV_HPP__
