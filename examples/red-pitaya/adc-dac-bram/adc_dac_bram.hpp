/// ADC DAC BRAM driver
///
/// (c) Koheron

#ifndef __DRIVERS_ADC_DAC_BRAM_HPP__
#define __DRIVERS_ADC_DAC_BRAM_HPP__

#include "server/hardware/memory_manager.hpp"

#include <array>

constexpr uint32_t dac_size = mem::dac_range/sizeof(uint32_t);
constexpr uint32_t adc_size = mem::adc_range/sizeof(uint32_t);

class AdcDacBram
{
  public:
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
        dac_map.write_array(data);
    }

    std::array<uint32_t, adc_size> get_adc() {
        trigger_acquisition();
        return adc_map.read_array<uint32_t, adc_size>();
    }

 private:
    hw::Memory<mem::control>& ctl = hw::get_memory<mem::control>();
    hw::Memory<mem::adc>& adc_map = hw::get_memory<mem::adc>();
    hw::Memory<mem::dac>& dac_map = hw::get_memory<mem::dac>();

}; // class AdcDacBram

#endif // __DRIVERS_ADC_DAC_BRAM_HPP__
