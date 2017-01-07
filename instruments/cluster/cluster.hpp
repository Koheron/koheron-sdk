/// AdcDac driver
///
/// (c) Koheron

#ifndef __DRIVERS_CLUSTER_HPP__
#define __DRIVERS_CLUSTER_HPP__

#include <context.hpp>

class Cluster
{
  public:
    Cluster(Context& ctx)
    : cfg(ctx.mm.get<mem::config>())
    , sts(ctx.mm.get<mem::status>())
    , cfg_clk(ctx.mm.get<mem::cfg_clk>())
    {}

    void phase_shift(uint32_t incdec) {
        cfg_clk.write_mask<0, (1 << 2) + (1 << 3)>((1 << 2) + (incdec << 3));
        cfg_clk.clear_bit<0, 2>();
    }

    // Trigger

    void trig_pulse() {
        cfg.set_bit<reg::trigger, 0>();
        cfg.clear_bit<reg::trigger, 0>();
    }

    // Pulse generator

    void set_pulse_generator(uint32_t pulse_width, uint32_t pulse_period) {
        cfg.write<reg::pulse_width>(pulse_width);
        cfg.write<reg::pulse_period>(pulse_period);
    }

    void clk_sel(uint32_t clk_sel) {
        cfg_clk.write_bit<0, 1>(1);
        cfg_clk.write_bit<0, 0>(clk_sel);
        cfg_clk.write_bit<0, 1>(0);
    }

    uint32_t read_sata() {
        return sts.read<reg::sts_sata>();
    }

    void cfg_sata(uint32_t sata_out_sel, uint32_t trig_delay) {
        cfg.write<reg::cfg_sata>(sata_out_sel + (trig_delay << 1));
    }

    void set_phase_increment(uint64_t phase_incr) {
        cfg.write<reg::phase_incr0, uint64_t>(phase_incr);
    }

    void set_freq(double freq_hz) {
        constexpr double factor = (uint64_t(1) << 48) / double(prm::adc_clk);
        set_phase_increment(uint64_t(factor * freq_hz));
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
    Memory<mem::cfg_clk>& cfg_clk;
};

#endif // __DRIVERS_CLUSTER_HPP__
