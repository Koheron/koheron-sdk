/// Blink driver
///
/// (c) Koheron

#ifndef __DRIVERS_BLINK_HPP__
#define __DRIVERS_BLINK_HPP__

#include <drivers/lib/dev_mem.hpp>
#include <drivers/addresses.hpp>

class Blink
{
  public:
    Blink(DevMem& dvm_)
    : dvm(dvm_)
    , cfg(dvm.get<CONFIG_MEM>())
    , sts(dvm.get<STATUS_MEM>())
    {}

    void set_dac(uint32_t dac0, uint32_t dac1) {
        cfg.write<DAC0_OFF>(dac0);
        cfg.write<DAC1_OFF>(dac1);
    }

    std::tuple<uint32_t, uint32_t>
    get_adc() {
        return std::make_tuple(sts.read<ADC0_OFF>(), sts.read<ADC1_OFF>());
    }

  private:
    DevMem& dvm;
    MemoryMap<CONFIG_MEM>& cfg;
    MemoryMap<STATUS_MEM>& sts;

}; // class Blink

#endif // __DRIVERS_BLINK_HPP__