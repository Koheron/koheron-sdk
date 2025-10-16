/// ADC DAC BRAM driver
///
/// (c) Koheron

#ifndef __EXAMPLES_ALPHA250_4_ADC_BRAM_HPP__
#define __EXAMPLES_ALPHA250_4_ADC_BRAM_HPP__

#include "server/runtime/syslog.hpp"
#include "server/hardware/memory_manager.hpp"

#include <array>
#include <cstdint>

class AdcBram
{
  public:
    void trigger_acquisition() {
        auto& ctl = hw::get_memory<mem::control>();
        ctl.set_bit<reg::trig, 0>();
        ctl.clear_bit<reg::trig, 0>();
    }

    uint32_t get_adc_size() {
        return adc0_size;
    }

    auto get_adc(uint32_t adc) {
        if (adc == 0) {
            return hw::get_memory<mem::adc0>().read_array<uint32_t, adc0_size>();
        } else if (adc == 1) {
            return hw::get_memory<mem::adc1>().read_array<uint32_t, adc1_size>();
        } else {
            log<ERROR>("AdcBram::get_adc invalid ADC reference\n");
            return std::array<uint32_t, adc0_size>{};
        }
    }

  private:
    static constexpr uint32_t adc0_size = mem::adc0_range / sizeof(uint32_t);
    static constexpr uint32_t adc1_size = mem::adc1_range / sizeof(uint32_t);
    static_assert(adc0_size == adc1_size);
}; // class AdcBram

#endif // __EXAMPLES_ALPHA250_4_ADC_BRAM_HPP__
