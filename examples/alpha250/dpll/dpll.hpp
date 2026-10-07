/// dpll driver
///
/// (c) Koheron

#ifndef __ALPHA250_DPLL_DPLL_HPP__
#define __ALPHA250_DPLL_DPLL_HPP__

#include "server/runtime/syslog.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/hardware/memory_manager.hpp"
#include "boards/alpha250/drivers/clock-generator.hpp"
#include "gain_control.hpp"
#include "p_path_control.hpp"
#include "monitor_revision.hpp"

#include <array>
#include <limits>
#include <cmath>
#include <cstdint>
#include <tuple>

class Dpll
{
  public:
    Dpll()
    : ctl(hw::get_memory<mem::control>())
    , sts(hw::get_memory<mem::status>())
    , clk_gen(rt::get_driver<ClockGenerator>())
    , gain_tables(ctl, sts, reg::gain_table_command, reg::gain_table_data0,
                  reg::gain_table_ack, reg::gain_table_banks, reg::gain_coefficients0)
    , p_paths(ctl, sts, reg::p_path_control, reg::p_fast_coeff0,
              reg::p_path_status0, reg::p_snapshot0, reg::integrators0)
    {
        static_assert(prm::adc_clk == 200000000 || prm::adc_clk == 250000000,
                      "DPLL supports 200 or 250 MHz sampling clocks");
        clk_gen.set_sampling_frequency(prm::adc_clk == 250000000 ? 1 : 0);
    }

    void set_integrator( uint32_t channel, uint32_t integrator_index, bool integrator_on) {
        dpll_monitor::ControlChange monitor_change;
        if (channel >= 2 || integrator_index >= 4) return;
        if ((integrator_index == 0 || integrator_index == 2) && !integrator_on && p_paths.select(channel, 0) != 0) {
            log<ERROR>("DPLL failed to disable fast P\n");
            return;
        }
        ctl.write_bit_reg(reg::integrators0 + 4*channel, integrator_index, integrator_on);
    }

    void set_dac_output(uint32_t channel, uint32_t sel) {
        dpll_monitor::ControlChange monitor_change;
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

        logf("DAC{} output set to {}\n", channel, sel);
    }

    void set_dds_freq(uint32_t channel, double freq_hz) {
        dpll_monitor::ControlChange monitor_change;
        if (channel >= 2) {
            log<ERROR>("FFT::set_dds_freq invalid channel\n");
            return;
        }

        if (std::isnan(freq_hz)) {
            log<ERROR>("FFT::set_dds_freq Frequency is NaN\n");
            return;
        }

        double fs_adc = clk_gen.get_dac_sampling_freq();

        if (freq_hz > fs_adc / 2) {
            freq_hz = fs_adc / 2;
        }

        if (freq_hz < 0.0) {
            freq_hz = 0.0;
        }

        // Frequency changes invalidate the captured lock reference. Return to
        // the accurate source before changing the DDS; enable Fast manually.
        if (p_paths.select(channel, 0) != 0) {
            log<ERROR>("DPLL failed to return P to accurate mode\n");
            return;
        }

        double factor = (uint64_t(1) << 48) / fs_adc;

        //ctl.write<reg::phase_incr0, uint64_t>(phase_incr);

        ctl.write_reg<uint64_t>(reg::phase_incr0 + 8 * channel, uint64_t(factor * freq_hz));
        dds_freq[channel] = freq_hz;

        logf("fs {}, channel {}, ref. frequency set to {}\n", fs_adc, channel, freq_hz);
    }

    void set_p_gain(uint32_t channel, int32_t p_gain_) {
        if (!set_integer_gain(channel, 0, p_gain_)) return;
        ctl.write_reg<int32_t>(reg::p_gain0 + 4*channel, p_gain_);
        logf("channel {}, p_gain set to {}\n", channel, p_gain_);
    }

    void set_pi_gain(uint32_t channel, int32_t pi_gain_) {
        if (!set_integer_gain(channel, 1, pi_gain_)) return;
        ctl.write_reg<int32_t>(reg::pi_gain0 + 4*channel, pi_gain_);
        logf("channel {}, pi_gain set to {}\n", channel, pi_gain_);
    }

    void set_i2_gain(uint32_t channel, int32_t i2_gain_) {
        if (!set_integer_gain(channel, 2, i2_gain_)) return;
        ctl.write_reg<int32_t>(reg::i2_gain0 + 4*channel, i2_gain_);
        logf("channel {}, i2_gain set to {}\n", channel, i2_gain_);
    }

    void set_i3_gain(uint32_t channel, int32_t i3_gain_) {
        if (!set_integer_gain(channel, 3, i3_gain_)) return;
        ctl.write_reg<int32_t>(reg::i3_gain0 + 4*channel, i3_gain_);
    }

    auto get_control_parameters() {
        if (!gain_tables.synchronize()) log<ERROR>("DPLL gain programmer timeout\n");
        return std::tuple{
            dds_freq[0],
            dds_freq[1],
            legacy_gain(0, 0), legacy_gain(1, 0),
            legacy_gain(0, 1), legacy_gain(1, 1),
            legacy_gain(0, 2), legacy_gain(1, 2),
            legacy_gain(0, 3), legacy_gain(1, 3),
            ctl.read<reg::integrators0>(),
            ctl.read<reg::integrators1>()
        };
    }

    auto get_dac_outputs() {
        const auto sel = ctl.read<reg::dac_sel>();
        return std::tuple{sel & 0b111U, (sel >> 3) & 0b111U};
    }

    // New RPCs are appended so existing command IDs remain unchanged.
    // step is an integer number of sixteenth-octave increments, 0..496.
    int32_t set_geometric_gain(uint32_t channel, uint32_t gain, int32_t sign, uint32_t step) {
        dpll_monitor::ControlChange monitor_change;
        int64_t coefficient = 0;
        if (channel >= 2 || gain >= 4 || !dpll_gain::geometric(sign, step, coefficient)) return -1;
        return gain_tables.program(channel, gain, coefficient) ? 0 : -2;
    }

    std::array<double, 8> get_gain_values() {
        std::array<double, 8> values{};
        if (!gain_tables.synchronize()) {
            log<ERROR>("DPLL gain programmer timeout\n");
            values.fill(std::numeric_limits<double>::quiet_NaN());
            return values;
        }
        for (uint32_t gain = 0; gain < 4; ++gain)
            for (uint32_t channel = 0; channel < 2; ++channel)
                values[2 * gain + channel] = double(gain_tables.coefficient(channel, gain)) / 2048.0;
        return values;
    }

    double get_dds_freq(uint32_t channel) {
        if (channel >= 2) return std::numeric_limits<double>::quiet_NaN();
        std::shared_lock lock(dpll_monitor::controls_mutex);
        return double(ctl.read_reg<uint64_t>(reg::phase_incr0 + 8 * channel) &
            ((uint64_t{1} << 48) - 1)) * double(prm::adc_clk) / double(uint64_t{1} << 48);
    }

    // Appended RPCs preserve all existing command IDs. 0=Accurate, 1=Fast.
    int32_t set_p_mode(uint32_t channel, uint32_t mode) {
        dpll_monitor::ControlChange monitor_change;
        return p_paths.select(channel, mode);
    }

    std::array<uint32_t, 2> get_p_path_status() {
        return {sts.read<reg::p_path_status0>(), sts.read<reg::p_path_status1>()};
    }

  private:
    bool set_integer_gain(uint32_t channel, uint32_t gain, int32_t value) {
        dpll_monitor::ControlChange monitor_change;
        if (channel >= 2) return false;
        if (!gain_tables.program(channel, gain, int64_t(value) * 2048)) {
            log<ERROR>("DPLL gain table update failed\n");
            return false;
        }
        return true;
    }

    // Preserve the legacy tuple layout. Fractional geometric gains are rounded
    // here; get_gain_values() reports the exact applied coefficients instead.
    uint32_t legacy_gain(uint32_t channel, uint32_t gain) {
        return static_cast<uint32_t>(static_cast<int32_t>(
            std::llround(double(gain_tables.coefficient(channel, gain)) / 2048.0)));
    }

    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::status>& sts;
    ClockGenerator& clk_gen;
    dpll_gain::Tables<hw::Memory<mem::control>, hw::Memory<mem::status>> gain_tables;
    dpll_p::Paths<hw::Memory<mem::control>, hw::Memory<mem::status>> p_paths;

    std::array<double, 2> dds_freq = {{0.0, 0.0}};
};

#endif // __ALPHA250_DPLL_DPLL_HPP__
