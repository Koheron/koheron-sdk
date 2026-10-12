/// VCO driver
///
/// (c) Koheron

#ifndef KOHERON_ALPHA250_VCO_HPP
#define KOHERON_ALPHA250_VCO_HPP

#include <context.hpp>
#include <cmath>
#include <cstdint>

class VCO
{
  public:
    VCO(Context& ctx) : ctl(ctx.mm.get<mem::control>()) {}

    // Carrier offset in Hz, from DC to Nyquist at the fixed 250 MS/s rate.
    void set_dds_freq(uint32_t channel, double freq_hz) {
        if (channel >= 2 || !std::isfinite(freq_hz) ||
            freq_hz < 0.0 || freq_hz > double(prm::adc_clk) / 2.0) {
            return;
        }
        constexpr double factor = (uint64_t(1) << 48) / double(prm::adc_clk);
        ctl.write_reg<uint64_t>(reg::phase_incr0 + 8 * channel, uint64_t(factor * freq_hz));
    }

    // Right-shift count: each step halves the ADC's frequency contribution.
    void set_vco_gain(uint32_t channel, uint32_t vco_gain) {
        if (channel >= 2 || vco_gain > 48) {
            return;
        }
        ctl.write_reg<uint32_t>(reg::vco_gain0 + 4 * channel, vco_gain);
    }

  private:
    Memory<mem::control>& ctl;
};

#endif // KOHERON_ALPHA250_VCO_HPP
