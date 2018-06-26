#ifndef __ALPHA_DRIVERS_PRECISION_ADC_HPP__
#define __ALPHA_DRIVERS_PRECISION_ADC_HPP__

#include <context.hpp>

#include <array>
#include <chrono>
#include <thread>
#include <mutex>

class PrecisionAdc
{
  public:
    PrecisionAdc(Context& ctx_)
    : ctx(ctx_)
    , spi(ctx.spi.get("spidev1.0"))
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

    uint32_t get_device_id() {
        return read(0x05, 1);
    }

    const auto& get_adc_values() {
        std::lock_guard<std::mutex> lock(acquisition_mtx);
        return analog_inputs_data;
    }

  private:
    Context& ctx;
    SpiDev& spi;

    static constexpr uint8_t channel_num = 8;
    std::array<float, channel_num> analog_inputs_data;
    std::atomic<bool> adc_acquisition_started{false};

    std::thread adc_read_thread;
    std::mutex acquisition_mtx;
    void adc_acquisition_thread();
    void start_adc_acquisition();

    uint32_t read(uint32_t address, uint32_t len) {
        uint8_t cmd[] = {uint8_t((0 << 7) + (1 << 6) + (address & 0x3F))};
        uint8_t data[4];
        spi.transfer(cmd, data, len + 1);
        switch (len) {
            case 1: return data[1];
            case 2: return (data[1] << 8) + data[2];
            case 3: return (data[1] << 16) + (data[2] << 8) + data[3];
            default: return 0;
        }
    }

    void write(uint32_t address, uint32_t value, uint32_t len) {
        uint8_t cmd[4];
        uint8_t data[4];
        cmd[0] = uint8_t((0 << 7) + (0 << 6) + (address & 0x3F));
        switch (len) {
            case 1: {
                cmd[1] = value & 0xFF;
                break;
            };
            case 2: {
                cmd[1] = (value >> 8) & 0xFF;
                cmd[2] = value & 0xFF;
                break;
            };
            case 3: {
                cmd[1] = (value >> 16) & 0xFF;
                cmd[2] = (value >> 8) & 0xFF;
                cmd[3] = value & 0xFF;
                break;
            };
            default: break;
        }

        spi.transfer(cmd, data, len + 1);
    }

    void set_channels(int32_t setup_idx) {
        constexpr uint32_t CHANNEL_EN = 1;

        for (uint32_t i = 0; i < channel_num; i++) {
            // Acquisition pairs are (1,2), (3,4), ..., (2i, 2i+1)
            uint32_t AINP = 2 * i;
            uint32_t AINM = 2 * i + 1;
            write(0x09 + i, (CHANNEL_EN << 15) + (setup_idx << 12) + (AINP << 5) + AINM, 2);
        }
    }

    void set_adc_control() {
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

    void set_configuration(int32_t setup_idx) {
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

    void set_filter(int32_t setup_idx) {
        constexpr uint32_t Filter       = 0;
        constexpr uint32_t REJ60        = 0;
        constexpr uint32_t POST_FILTER  = 0b000;
        constexpr uint32_t SINGLE_CYCLE = 0;
        constexpr uint32_t FS           = 60;

        write(0x21 + setup_idx, (Filter << 21) + (REJ60 << 20) + (POST_FILTER << 17)
              + (SINGLE_CYCLE << 16) + (0 << 11) + (FS << 0), 3);
    }
}; // class PrecisionAdc

inline void PrecisionAdc::start_adc_acquisition() {
    if (! adc_acquisition_started) {
        analog_inputs_data.fill(0.0);
        adc_read_thread = std::thread{&PrecisionAdc::adc_acquisition_thread, this};
        adc_read_thread.detach();
    }
}

inline void PrecisionAdc::adc_acquisition_thread() {
    using namespace std::chrono_literals;

    adc_acquisition_started = true;

    while (adc_acquisition_started) {
        uint8_t cmd[] = {uint8_t((0 << 7) + (1 << 6) + (0x02 & 0x3F))};
        uint8_t data[5];
        spi.transfer(cmd, data, 6);

        uint32_t raw_data = (data[1] << 16) + (data[2] << 8) + data[3];
        uint8_t channel = data[4] & 0xF;

        if (channel < channel_num) {
            constexpr float vref = 1.25; // Volts

            std::lock_guard<std::mutex> lock(acquisition_mtx);
            analog_inputs_data[channel] = vref * (float(raw_data) / (1 << 23) - 1);
        } else {
            ctx.log<WARNING>("Unexpected channel (# %u) received while reading precision ADC\n", channel);
        }

        std::this_thread::sleep_for(10ms);
    }
}

#endif // __ALPHA_DRIVERS_PRECISION_ADC_HPP__
