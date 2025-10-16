/// VCO driver
///
/// (c) Koheron

#ifndef __ALPHA15_VCO_HPP__
#define __ALPHA15_VCO_HPP__

#include "server/hardware/memory_manager.hpp"

#include <cstdint>

class VCO
{
  public:
    VCO() : ctl(hw::get_memory<mem::control>()){}

    void set_dds_freq(uint32_t channel, double freq_hz) {
        constexpr double factor = (uint64_t(1) << 48) / double(prm::adc_clk);
        ctl.write_reg<uint64_t>(reg::phase_incr0 + 8 * channel, uint64_t(factor * freq_hz));
    }

    void set_vco_gain(uint32_t channel, uint32_t vco_gain) {
        ctl.write_reg<uint32_t>(reg::vco_gain0 + 4 * channel, vco_gain);
    }

  private:
    hw::Memory<mem::control>& ctl;
};

#endif // __ALPHA15_VCO_HPP__
