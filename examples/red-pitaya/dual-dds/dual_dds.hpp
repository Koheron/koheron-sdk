/// DDS FFT driver
///
/// (c) Koheron

#ifndef __DRIVERS_DUAL_DDS_HPP__
#define __DRIVERS_DUAL_DDS_HPP__

#include <context.hpp>
#include <atomic>
#include <thread>
#include <chrono>

class DualDDS
{
  public:
    DualDDS(Context& ctx)
    : ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    {
    }

    void set_dds_freq(uint32_t channel, double freq_hz) {
        constexpr double factor = (uint64_t(1) << 48) / double(prm::adc_clk);
        ctl.write_reg<uint64_t>(reg::phase_incr0 + 8 * channel, uint64_t(factor * freq_hz));
        dds_freq[channel] = freq_hz;
    }

    auto get_control_parameters() {
        return std::make_tuple(
            dds_freq[0],
            dds_freq[1]
        );
    }


  private:
    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::status>& sts;

    std::array<double, 2> dds_freq = {{0, 0}};

};


#endif // __DRIVERS_DDS_FFT_HPP__
