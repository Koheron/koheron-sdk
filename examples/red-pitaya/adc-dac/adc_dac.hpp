/// AdcDac driver
///
/// (c) Koheron

#ifndef __DRIVERS_ADC_DAC_HPP__
#define __DRIVERS_ADC_DAC_HPP__

#include <context.hpp>

class AdcDac
{
  public:
    AdcDac(Context& ctx)
    : ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    {}

    void set_dac(uint32_t dac_value, uint32_t dac_channel) {
        ctl.write_reg(reg::dac0 + 4 * (dac_channel % 2), dac_value);
    }

    void set_dac_0(uint32_t dac0) {
        ctl.write<reg::dac0>(dac0);
    }

    void set_dac_1(uint32_t dac1) {
        ctl.write<reg::dac1>(dac1);
    }

    auto get_adc() {
        // Convert from two-complement to int32
        int32_t adc0 = ((static_cast<int32_t>(sts.read<reg::adc0>()) - 8192) % 16384) - 8192;
        int32_t adc1 = ((static_cast<int32_t>(sts.read<reg::adc1>()) - 8192) % 16384) - 8192;
        return std::make_tuple(adc0, adc1);
    }

  private:
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
};

#endif // __DRIVERS_ADC_DAC_HPP__
