#ifndef __ALPHA250_DRIVERS_PRECISION_ADC_HPP__
#define __ALPHA250_DRIVERS_PRECISION_ADC_HPP__

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <mutex>

namespace hw { class SpiDev; }

class PrecisionAdc
{
  public:
    PrecisionAdc();

    uint32_t get_device_id() {
        return read(0x05, 1);
    }

    auto get_adc_values() {
        std::lock_guard<std::mutex> lock(acquisition_mtx);
        return analog_inputs_data;
    }

  private:
    hw::SpiDev& spi;

    static constexpr uint8_t channel_num = 8;
    std::array<float, channel_num> analog_inputs_data{};
    std::atomic<bool> adc_acquisition_started{false};

    std::thread adc_read_thread;
    std::mutex acquisition_mtx;

    void adc_acquisition_thread();
    void start_adc_acquisition();
    uint32_t read(uint32_t address, uint32_t len);
    void write(uint32_t address, uint32_t value, uint32_t len);
    void set_channels(int32_t setup_idx);
    void set_adc_control();
    void set_configuration(int32_t setup_idx);
    void set_filter(int32_t setup_idx);
}; // class PrecisionAdc

#endif // __ALPHA250_DRIVERS_PRECISION_ADC_HPP__
