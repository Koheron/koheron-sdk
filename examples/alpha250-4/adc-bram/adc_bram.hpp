/// ADC DAC BRAM driver
///
/// (c) Koheron

#ifndef __EXAMPLES_ALPHA250_4_ADC_BRAM_HPP__
#define __EXAMPLES_ALPHA250_4_ADC_BRAM_HPP__

#include <context.hpp>
#include <array>

class AdcBram
{
  public:
    AdcBram(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , adc0_map(ctx.mm.get<mem::adc0>())
    , adc1_map(ctx.mm.get<mem::adc1>())
    {}

    void trigger_acquisition() {
        ctl.set_bit<reg::trig, 0>();
        ctl.clear_bit<reg::trig, 0>();
    }

    uint32_t get_adc_size() {
        return adc0_size;
    }

    auto get_adc(uint32_t adc) {
        if (adc == 0) {
            return adc0_map.read_array<uint32_t, adc0_size>();
        } else if (adc == 1) {
            return adc1_map.read_array<uint32_t, adc1_size>();
        } else {
            ctx.log<ERROR>("AdcBram::get_adc invalid ADC reference\n");
            return std::array<uint32_t, adc0_size>{};
        }
    }

  private:
    static constexpr uint32_t adc0_size = mem::adc0_range / sizeof(uint32_t);
    static constexpr uint32_t adc1_size = mem::adc1_range / sizeof(uint32_t);
    static_assert(adc0_size == adc1_size, "");

    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::adc0>& adc0_map;
    Memory<mem::adc1>& adc1_map;
}; // class AdcBram

#endif // __EXAMPLES_ALPHA250_4_ADC_BRAM_HPP__
