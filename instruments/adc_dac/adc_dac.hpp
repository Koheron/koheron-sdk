/// AdcDac driver
///
/// (c) Koheron

#ifndef __DRIVERS_ADC_DAC_HPP__
#define __DRIVERS_ADC_DAC_HPP__

#include <drivers/lib/memory_manager.hpp>
#include <drivers/memory.hpp>

class AdcDac
{
  public:
    AdcDac(MemoryManager& mm)
    : cfg(mm.get<mem::config>())
    , sts(mm.get<mem::status>())
    {}

    void set_dac(uint32_t dac0, uint32_t dac1) {
        cfg.write<reg::dac0>(dac0);
        cfg.write<reg::dac1>(dac1);
    }

    auto get_adc() {
        // Convert from two-complement to int32
        int32_t adc0 = ((sts.read<reg::adc0>() - 8192) % 16384) - 8192;
        int32_t adc1 = ((sts.read<reg::adc1>() - 8192) % 16384) - 8192;
        return std::make_tuple(adc0, adc1);
    }

  private:
    Memory<mem::config>& cfg;
    Memory<mem::status>& sts;
};

#endif // __DRIVERS_ADC_DAC_HPP__
