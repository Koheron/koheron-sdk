/// DDS driver
///
/// (c) Koheron

#ifndef __DRIVERS_DDS_HPP__
#define __DRIVERS_DDS_HPP__

#include <context.hpp>
#include <boards/alpha250/drivers/clock-generator.hpp>

#include <array>
#include <limits>
#include <cmath>

#include <scicpp/core.hpp>

class Dds
{
  public:
    Dds(Context& _ctx)
    : ctx(_ctx)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , clk_gen(ctx.get<ClockGenerator>())
    {
        clk_gen.set_sampling_frequency(0);
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

        double fs_adc = clk_gen.get_dac_sampling_freq();

        if (freq_hz > fs_adc / 2.0) {
            freq_hz = fs_adc / 2.0;
        }

        if (freq_hz < 0.0) {
            freq_hz = 0.0;
        }

        double factor = (uint64_t(1) << 48) / fs_adc;

        ctl.write_reg<uint64_t>(reg::phase_incr0 + 8 * channel, uint64_t(factor * freq_hz));
        dds_freq[channel] = freq_hz;

        ctx.logf<INFO>("fs: {}, channel {}, ref. frequency set to {}\n", fs_adc, channel, freq_hz);
    }

    auto get_dds_freq(uint32_t channel) {
        return dds_freq[channel];
    }

    template<typename T=float>
    auto get_lo_freq(uint32_t channel) {
        return scicpp::units::frequency<T>(dds_freq[channel]);
    }

  private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    ClockGenerator& clk_gen;

    std::array<double, 2> dds_freq = {{0.0, 0.0}};
};

#endif // __DRIVERS_DDS_HPP__
