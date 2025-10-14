#include "./precision-adc.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/hardware/spi_manager.hpp"
#include "server/utilities/endian_utils.hpp"

#include <chrono>
#include <span>

PrecisionAdc::PrecisionAdc()
: spi(services::require<hw::SpiManager>().get("spidev1.0"))
{
    if (! spi.is_ok()) {
        return;
    }

    spi.set_mode(SPI_MODE_3);
    spi.set_speed(10000000);

    constexpr int32_t setup_idx = 0; // All channels on Setup 0
    set_channels(setup_idx);
    set_configuration(setup_idx);
    set_filter(setup_idx);
    set_adc_control();

    start_adc_acquisition();
}

uint32_t PrecisionAdc::read(uint32_t address, uint32_t len) {
    if (len == 0 || len > 3) {
        return 0;
    }

    std::array<uint8_t, 4> tx{};  // [cmd, dmy/dmy/dmy]
    std::array<uint8_t, 4> rx{};  // [dmy, b1, b2, b3]

    tx[0] = static_cast<uint8_t>((0u << 7) | (1u << 6) | (address & 0x3Fu));

    if (spi.transfer(tx, rx, len + 1) < 0) {
        return 0;
    }

    auto payload = std::span<const uint8_t>(rx).subspan(1, static_cast<std::size_t>(len));
    return koheron::from_big_endian_bytes<std::uint32_t>(payload, len);
}

void PrecisionAdc::write(uint32_t address, uint32_t value, uint32_t len) {
    if (len == 0 || len > 3) {
        log<ERROR>("PrecisionAdc: write invalid length");
        return;
    }

    std::array<uint8_t, 4> tx{};  // [cmd, b1, b2, b3]
    std::array<uint8_t, 4> rx{};
    tx[0] = static_cast<uint8_t>((0u << 7) | (0u << 6) | (address & 0x3Fu));
    koheron::to_big_endian_bytes(value, tx, static_cast<std::size_t>(len), 1);
    spi.transfer(tx, rx, len + 1);
}

void PrecisionAdc::set_channels(int32_t setup_idx) {
    constexpr uint32_t CHANNEL_EN = 1;

    for (uint32_t i = 0; i < channel_num; i++) {
        // Acquisition pairs are (1,2), (3,4), ..., (2i, 2i+1)
        uint32_t AINP = 2 * i;
        uint32_t AINM = 2 * i + 1;
        write(0x09 + i, (CHANNEL_EN << 15) + (setup_idx << 12) + (AINP << 5) + AINM, 2);
    }
}

void PrecisionAdc::set_adc_control() {
    constexpr uint32_t DOUT_RDY_DEL = 0;
    constexpr uint32_t CONT_READ    = 0;
    constexpr uint32_t DATA_STATUS  = 1; // Status register is transferred with the data
    constexpr uint32_t CS_EN        = 0;
    constexpr uint32_t REF_EN       = 0;
    constexpr uint32_t POWER_MODE   = 2; // 0: low power, 1: mid power, 2: full power
    constexpr uint32_t Mode         = 0; // 0: Continuous conversion mode
    constexpr uint32_t CLK_SEL      = 0; // Internal clock not available on CLK

    write(0x01, (DOUT_RDY_DEL << 12) + (CONT_READ << 11) + (DATA_STATUS << 10) + (CS_EN << 9)
            + (REF_EN << 8) + (POWER_MODE << 6) + (Mode << 2) + (CLK_SEL << 0), 2);
}

void PrecisionAdc::set_configuration(int32_t setup_idx) {
    constexpr uint32_t Bipolar  = 1; // Bipolar operation
    constexpr uint32_t REF_BUFP = 1; // Positive reference buffered
    constexpr uint32_t REF_BUFM = 1; // Negative reference buffered
    constexpr uint32_t AIN_BUFP = 0; // Positive analog input buffered
    constexpr uint32_t AIN_BUFM = 0; // Positive analog input buffered
    constexpr uint32_t REF_SEL  = 0; // 0: External reference
    constexpr uint32_t PGA      = 0; // PGA gain

    write(0x19 + setup_idx, (Bipolar << 11) + (REF_BUFP << 8) + (REF_BUFM << 7) + (AIN_BUFP << 6)
            + (AIN_BUFM << 5) + (REF_SEL << 3) + (PGA << 0), 2);
}

void PrecisionAdc::set_filter(int32_t setup_idx) {
    constexpr uint32_t Filter       = 0;
    constexpr uint32_t REJ60        = 0;
    constexpr uint32_t POST_FILTER  = 0b000;
    constexpr uint32_t SINGLE_CYCLE = 0;
    constexpr uint32_t FS           = 60;

    write(0x21 + setup_idx, (Filter << 21) + (REJ60 << 20) + (POST_FILTER << 17)
            + (SINGLE_CYCLE << 16) + (0 << 11) + (FS << 0), 3);
}

void PrecisionAdc::start_adc_acquisition() {
    if (! adc_acquisition_started) {
        analog_inputs_data.fill(0.0);
        adc_read_thread = std::thread{&PrecisionAdc::adc_acquisition_thread, this};
        adc_read_thread.detach();
    }
}

void PrecisionAdc::adc_acquisition_thread() {
    using namespace std::chrono_literals;

    adc_acquisition_started = true;

    while (adc_acquisition_started) {
        constexpr uint8_t cmd_byte = static_cast<uint8_t>((0u << 7) | (1u << 6) | (0x02u & 0x3Fu));
        std::array<uint8_t, 5> tx{};  // [cmd, dmy, dmy, dmy, dmy]
        std::array<uint8_t, 5> rx{};  // [dmy, b1,  b2,  b3,  ch ]
        tx[0] = cmd_byte;

        if (spi.transfer(tx, rx) < 0) {
            log<WARNING>("PrecisionAdc: An error occured during read\n");
            std::this_thread::sleep_for(10ms);
            continue;
        }

        const uint32_t raw_data = (uint32_t(rx[1]) << 16) | (uint32_t(rx[2]) << 8) | uint32_t(rx[3]);
        const uint8_t  channel  = rx[4] & 0x0F;

        if (channel < channel_num) {
            constexpr float vref = 1.25f; // Volts
            std::lock_guard lock(acquisition_mtx);
            analog_inputs_data[channel] = vref * (float(raw_data) / float(1u << 23) - 1.0f);
        } else {
            logf<WARNING>("PrecisionAdc: Unexpected channel (# {}) received\n", channel);
        }

        std::this_thread::sleep_for(10ms);
    }
}