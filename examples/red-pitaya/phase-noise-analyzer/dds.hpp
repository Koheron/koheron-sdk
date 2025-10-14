/// DDS driver
///
/// (c) Koheron

#ifndef __DRIVERS_DDS_HPP__
#define __DRIVERS_DDS_HPP__

#include <context.hpp>

#include <array>
#include <limits>
#include <cmath>

class Dds
{
  public:
    Dds(Context& _ctx)
    : ctx(_ctx)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    {
        // Phase accumulator on
        ctl.write_mask<reg::cordic, 0b11>(0b11);
    }

    void set_dds_freq(uint32_t channel, double freq_hz) {

        if (channel >= 2) {
            ctx.log<ERROR>("FFT::set_dds_freq invalid channel\n");
            return;
        }

        if (std::isnan(freq_hz)) {
            ctx.log<ERROR>("FFT::set_dds_freq Frequency is NaN\n");
            return;
        }

        if (freq_hz > double(prm::adc_clk)/ 2) {
            freq_hz = double(prm::adc_clk) / 2;
        }

        if (freq_hz < 0.0) {
            freq_hz = 0.0;
        }

        double factor = (uint64_t(1) << 48) / double(prm::adc_clk);

        //ctl.write<reg::phase_incr0, uint64_t>(phase_incr);

        ctl.write_reg<uint64_t>(reg::phase_incr0 + 8 * channel, uint64_t(factor * freq_hz));
        dds_freq[channel] = freq_hz;

        ctx.log<INFO>("fs %lf , channel %u, ref. frequency set to %lf \n", double(prm::adc_clk), channel, freq_hz);
    }

  private:
    Context& ctx;
    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::status>& sts;

    std::array<double, 2> dds_freq = {{0.0, 0.0}};

};

#endif // __DRIVERS_DDS_HPP__
