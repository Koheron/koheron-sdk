/// AdcDac driver
///
/// (c) Koheron

#ifndef __DRIVERS_CLUSTER_HPP__
#define __DRIVERS_CLUSTER_HPP__

#include <drivers/lib/memory_manager.hpp>
#include <drivers/memory.hpp>

class Cluster
{
  public:
    Cluster(MemoryManager& mm)
    : cfg(mm.get<mem::config>())
    , sts(mm.get<mem::status>())
    , cfg_clk(mm.get<mem::cfg_clk>())
    , mmcm(mm.get<mem::mmcm>())
    {}

    /*
    // http://www.xilinx.com/support/documentation/ip_documentation/clk_wiz/v5_1/pg065-clk-wiz.pdf
    void set_mmcm_phase(int32_t phase) {
        mmcm.write<0x20C, int32_t>(phase);
        while (mmcm.read<0x04>() == 0) {
        }
        mmcm.write<0x25C>(0x7);
        mmcm.write<0x25C>(0x2);
    }
    */

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
    Memory<mem::mmcm>& mmcm;
};

#endif // __DRIVERS_CLUSTER_HPP__
