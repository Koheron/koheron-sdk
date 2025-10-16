/// ADC DAC BRAM driver
///
/// (c) Koheron

#ifndef __DRIVERS_ADC_DAC_BRAM_HPP__
#define __DRIVERS_ADC_DAC_BRAM_HPP__

#include "server/runtime/services.hpp"
#include "server/hardware/memory_manager.hpp"

#include <cstdint>
#include <array>

class AdcDacBram
{
  public:
    static constexpr uint32_t dac_size = mem::dac_range / sizeof(uint32_t);
    static constexpr uint32_t adc_size = mem::adc_range / sizeof(uint32_t);

    AdcDacBram() : mm(services::require<hw::MemoryManager>()) {}

    void trigger_acquisition() {
        auto& ctl = mm.get<mem::control>();
        ctl.set_bit<reg::trig, 0>();
        ctl.clear_bit<reg::trig, 0>();
    }

    uint32_t get_dac_size() {
        return dac_size;
    }

    uint32_t get_adc_size() {
        return adc_size;
    }

    void set_dac_data(const std::array<uint32_t, AdcDacBram::dac_size>& data) {
        mm.get<mem::dac>().write_array(data);
    }

    auto get_adc() {
        trigger_acquisition();
        return mm.get<mem::adc>().read_array<uint32_t, adc_size>();
    }

  private:

    hw::MemoryManager& mm;
}; // class AdcDacBram

#endif // __DRIVERS_ADC_DAC_BRAM_HPP__
