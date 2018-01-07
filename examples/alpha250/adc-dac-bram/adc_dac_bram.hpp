/// ADC DAC BRAM driver
///
/// (c) Koheron

#ifndef __DRIVERS_ADC_DAC_BRAM_HPP__
#define __DRIVERS_ADC_DAC_BRAM_HPP__

#include <context.hpp>
#include <array>

constexpr uint32_t dac_size = mem::dac_range/sizeof(uint32_t);
constexpr uint32_t adc_size = mem::adc_range/sizeof(uint32_t);

class AdcDacBram
{
  public:
    AdcDacBram(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , adc_map(ctx.mm.get<mem::adc>())
    , dac_map(ctx.mm.get<mem::dac>())
    {
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
        dac_map.write_array(data);
    }

    std::array<uint32_t, adc_size> get_adc_data() {
        trigger_acquisition();
        return adc_map.read_array<uint32_t, adc_size>();
    }

 private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::adc>& adc_map;
    Memory<mem::dac>& dac_map;

}; // class AdcDacBram

#endif // __DRIVERS_ADC_DAC_BRAM_HPP__
