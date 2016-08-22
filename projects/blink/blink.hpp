/// Blink driver
///
/// (c) Koheron

#ifndef __DRIVERS_BLINK_HPP__
#define __DRIVERS_BLINK_HPP__

#include <drivers/lib/memory_manager.hpp>
#include <drivers/memory.hpp>

class Blink
{
  public:
    Blink(MemoryManager& mm)
    : cfg(mm.get<mem::config>())
    , sts(mm.get<mem::status>())
    {}

    void set_dac(uint32_t dac0, uint32_t dac1) {
        cfg.write<reg::dac0>(dac0);
        cfg.write<reg::dac1>(dac1);
    }

    std::tuple<uint32_t, uint32_t>
    get_adc() {
        return std::make_tuple(sts.read<reg::adc0>(), sts.read<reg::adc1>());
    }

  private:
    MemoryMap<mem::config>& cfg;
    MemoryMap<mem::status>& sts;

};

#endif // __DRIVERS_BLINK_HPP__
