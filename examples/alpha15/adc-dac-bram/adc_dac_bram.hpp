/// ADC DAC BRAM driver
///
/// (c) Koheron

#ifndef __DRIVERS_ADC_DAC_BRAM_HPP__
#define __DRIVERS_ADC_DAC_BRAM_HPP__

#include "server/runtime/syslog.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/hardware/memory_manager.hpp"
#include "boards/alpha15/drivers/clock-generator.hpp"

#include <array>
#include <cstdint>
#include <thread>
#include <chrono>

constexpr uint32_t dac_size = mem::dac_range / sizeof(uint32_t);
constexpr uint32_t adc_size = mem::adc0_range / sizeof(uint32_t);

class AdcDacBram
{
  public:
    AdcDacBram()
    : ctl(hw::get_memory<mem::control>())
    {
        const auto fs_adc = rt::get_driver<ClockGenerator>().get_adc_sampling_freq();
        t_acq[0] = int32_t(1E6 * adc_size / fs_adc[0]);
        t_acq[1] = int32_t(1E6 * adc_size / fs_adc[1]);
        logf("AdcDacBram: t_acq = {{{}, {}}} us\n", t_acq[0], t_acq[1]);
    }

    void trigger_acquisition() {
        ctl.set_bit<reg::trig, 0>();
        ctl.clear_bit<reg::trig, 0>();
    }

    uint32_t get_dac_size() {
        return dac_size;
    }

    uint32_t get_adc_size() {
        return adc_size;
    }

    void set_dac_data(const std::array<uint32_t, dac_size>& data) {
        hw::get_memory<mem::dac>().write_array(data);
    }

    auto get_adc(bool channel) {
        trigger_acquisition();
        std::this_thread::sleep_for(std::chrono::microseconds(t_acq[channel]));
        return channel ? hw::get_memory<mem::adc1>().read_array<uint32_t, adc_size>()
                       : hw::get_memory<mem::adc0>().read_array<uint32_t, adc_size>();
    }

    // IOs

    void set_b34_ios(uint32_t value) {
        ctl.write<reg::digital_outputs_b34>(value);
    }

 private:
    hw::Memory<mem::control>& ctl;

    // Acquisition time for each ADC channel (microseconds)
    std::array<int32_t, 2> t_acq;
}; // class AdcDacBram

#endif // __DRIVERS_ADC_DAC_BRAM_HPP__