/// VCO driver
///
/// (c) Koheron

#ifndef __DRIVERS_DUAL_DDS_HPP__
#define __DRIVERS_DUAL_DDS_HPP__

#include <context.hpp>
#include <atomic>
#include <thread>
#include <chrono>

class VCO
{
  public:
    VCO(Context& ctx)
    : ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    {
    }

    void set_dds_freq(uint32_t channel, double freq_hz) {
        constexpr double factor = (uint64_t(1) << 48) / double(prm::adc_clk);
        ctl.write_reg<uint64_t>(reg::phase_incr0 + 8 * channel, uint64_t(factor * freq_hz));
    }

    void set_vco_gain(uint32_t channel, uint32_t vco_gain) {
        ctl.write_reg<uint32_t>(reg::vco_gain0 + 4 * channel, vco_gain);
    }

  private:
    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::status>& sts;

};


#endif // __DRIVERS_VCO_HPP__
