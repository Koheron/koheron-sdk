/// (c) Koheron

#ifndef __ALPHA15_DRIVERS_EEPROM_HPP__
#define __ALPHA15_DRIVERS_EEPROM_HPP__

#include <thread>
#include <chrono>
#include <cstring>
#include <array>

#include "server/runtime/services.hpp"
#include "server/hardware/i2c_manager.hpp"

// http://ww1.microchip.com/downloads/en/DeviceDoc/21189K.pdf

namespace eeprom_map {
    namespace identifications {
        constexpr int32_t offset = 0x000;
        constexpr int32_t range = 0x100;
    }

    namespace precision_dac_calib {
        constexpr int32_t offset = 0x100;
        constexpr int32_t range = 0x100;
    }

    namespace rf_adc_ch0_calib {
        constexpr int32_t offset = 0x200;
        constexpr int32_t range = 0x100;
    }

    namespace rf_adc_ch1_calib {
        constexpr int32_t offset = 0x300;
        constexpr int32_t range = 0x100;
    }

    namespace clock_generator_calib {
        constexpr int32_t offset = 0x400;
        constexpr int32_t range = 0x100;
    }

    namespace rf_dac_ch0_calib {
        constexpr int32_t offset = 0x500;
        constexpr int32_t range = 0x100;
    }

    namespace rf_dac_ch1_calib {
        constexpr int32_t offset = 0x600;
        constexpr int32_t range = 0x100;
    }

    // Addresses above 0x1000 are reserved to user
    constexpr int32_t user_off = 0x1000;
}

class Eeprom
{
  public:
    Eeprom()
    : i2c(services::require<hw::I2cManager>().get("i2c-0"))
    {}

    int32_t set_serial_number(uint32_t sn) {
        std::array<uint32_t, 1> data = {sn};
        return write<eeprom_map::identifications::offset>(data);
    }

    uint32_t get_serial_number() {
        std::array<uint32_t, 1> data;
        read<eeprom_map::identifications::offset>(data);
        return data[0];
    }

    template<int32_t offset, typename T, uint32_t N>
    int32_t write(const std::array<T, N>& data)
    {
        constexpr uint32_t n_bytes = N * sizeof(T);
        static_assert(offset + n_bytes <= EEPROM_SIZE, "Write out of EEPROM");

        uint32_t bytes_written = 0;
        const uint8_t *begin = reinterpret_cast<const uint8_t*>(data.data());

        while (bytes_written < n_bytes) {
            // PAGESIZE cast required to compile in -O0.
            // Should not be required anymore in C++17.
            // https://stackoverflow.com/questions/40690260/undefined-reference-error-for-static-constexpr-member
            auto size = std::min(uint32_t(PAGESIZE), n_bytes - bytes_written);
            if (__write_packet(offset + bytes_written, begin + bytes_written, size) < 0) {
                    return -1;
            }
            bytes_written += size;
        }
        return n_bytes;
    }

    template<int32_t offset, typename T, uint32_t N>
    int32_t read(std::array<T, N>& data)
    {
        constexpr uint32_t n_bytes = N * sizeof(T);
        static_assert(offset + n_bytes <= EEPROM_SIZE, "Read out of EEPROM");

        uint8_t buffer[2];
        buffer[0] = static_cast<uint8_t>(offset >> 8);
        buffer[1] = static_cast<uint8_t>(offset & 0xFF);

        if (__write(buffer, 2) != 2) {
            return -1;
        }

        if (i2c.read(EEPROM_ADDR, data) != n_bytes) {
            return -1;
        }
        return n_bytes;
    }

  private:
    static constexpr int32_t EEPROM_ADDR = 0b1010100;
    static constexpr uint32_t PAGESIZE = 32;
    static constexpr uint32_t EEPROM_SIZE = 64 * 1024 / 8;

    hw::I2cDev& i2c;

    int __write(const uint8_t *buffer, int32_t n_bytes)
    {
        using namespace std::chrono_literals;
        constexpr int32_t max_retries = 5;

        int retries = 0;
        int err = -1;

        while (retries < max_retries) {
            // EEPROM does not acknowledge I2C when internal programming is occuring.
            // When it happens, the call to i2c.write returns ERRNO=1.

            err = i2c.write(EEPROM_ADDR, buffer, n_bytes);

            if (err != -1) {
                // Write success or error different from EEPROM busy
                return err;
            }

            // From datasheet write cycle time is 5ms
            std::this_thread::sleep_for(5ms);

            retries++;
        }

        return -1;
    }

    int32_t __write_packet(int32_t offset, const uint8_t *packet, int32_t len)
    {
        int32_t packet_len = len + 2;
        assert(packet_len <= int32_t(PAGESIZE) + 2);
        uint8_t buffer[PAGESIZE + 2];

        buffer[0] = static_cast<uint8_t>(offset >> 8);
        buffer[1] = static_cast<uint8_t>(offset & 0xFF);
        std::memcpy(buffer + 2, packet, static_cast<uint32_t>(len));

        if (__write(buffer, packet_len) != packet_len) {
            return -1;
        }

        return packet_len;
    }
};

#endif // __ALPHA15_DRIVERS_EEPROM_HPP__