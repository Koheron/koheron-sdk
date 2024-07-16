/// dpll driver
///
/// (c) Koheron

#ifndef __DRIVERS_DPLL_HPP__
#define __DRIVERS_DPLL_HPP__

#include <context.hpp>
#include <boards/alpha250/drivers/clock-generator.hpp>

#include <array>
#include <limits>
#include <cmath>

class Dpll
{
  public:
    Dpll(Context& _ctx)
    : ctx(_ctx)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , clk_gen(ctx.get<ClockGenerator>())
    {
        clk_gen.set_sampling_frequency(0);
    }

    void set_integrator( uint32_t channel, uint32_t integrator_index, bool integrator_on) {
        ctl.write_bit_reg(reg::integrators0 + 4*channel, integrator_index, integrator_on);
    }

    void set_dac_output(uint32_t channel, uint32_t sel) {
        // sel =
        // 0: fast_corr0
        // 1: fast_corr1
        // 2: phase0 (16 LSBs)
        // 3: phase1 (16 LSBs)
        // 4: phase0 (16 MSBs)
        // 5: phase1 (16 MSBs)
        // 6: DDS0
        // 7: DDS1
        if (channel == 0) {
            ctl.write_mask<reg::dac_sel, 0b000111>(sel);
        } else if (channel == 1) {
            ctl.write_mask<reg::dac_sel, 0b111000>(sel << 3);
        }
        ctx.log<INFO>("DAC%u output set to %u\n", channel, sel);
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

        if (freq_hz > fs_adc / 2) {
            freq_hz = fs_adc / 2;
        }

        if (freq_hz < 0.0) {
            freq_hz = 0.0;
        }

        double factor = (uint64_t(1) << 48) / fs_adc;

        //ctl.write<reg::phase_incr0, uint64_t>(phase_incr);

        ctl.write_reg<uint64_t>(reg::phase_incr0 + 8 * channel, uint64_t(factor * freq_hz));
        dds_freq[channel] = freq_hz;

        ctx.log<INFO>("fs %lf , channel %u, ref. frequency set to %lf \n", fs_adc, channel, freq_hz);
    }

    void set_p_gain(uint32_t channel, int32_t p_gain_) {
        ctl.write_reg<int32_t>(reg::p_gain0 + 4*channel, p_gain_);
        ctx.log<INFO>("channel %u, p_gain set to %d \n", channel, p_gain_);
    }

    void set_pi_gain(uint32_t channel, int32_t pi_gain_) {
        ctl.write_reg<int32_t>(reg::pi_gain0 + 4*channel, pi_gain_);
        ctx.log<INFO>("channel %u, pi_gain set to %d \n", channel, pi_gain_);
    }

    void set_i2_gain(uint32_t channel, int32_t i2_gain_) {
        ctl.write_reg<int32_t>(reg::i2_gain0 + 4*channel, i2_gain_);
        ctx.log<INFO>("channel %u, i2_gain set to %d \n", channel, i2_gain_);
    }

    void set_i3_gain(uint32_t channel, int32_t i3_gain_) {
        ctl.write_reg<int32_t>(reg::i3_gain0 + 4*channel, i3_gain_);
    }

    auto get_control_parameters() {
        return std::make_tuple(
            dds_freq[0],
            dds_freq[1],
            ctl.read<reg::p_gain0>(),
            ctl.read<reg::p_gain1>(),
            ctl.read<reg::pi_gain0>(),
            ctl.read<reg::pi_gain1>(),
            ctl.read<reg::i2_gain0>(),
            ctl.read<reg::i2_gain1>(),
            ctl.read<reg::i3_gain0>(),
            ctl.read<reg::i3_gain1>(),
            ctl.read<reg::integrators0>(),
            ctl.read<reg::integrators1>()
        );
    }

  private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    ClockGenerator& clk_gen;

    std::array<double, 2> dds_freq = {{0.0, 0.0}};

};

#endif // __DRIVER_DPLL_HPP__
