/// DDS driver
///
/// (c) Koheron

#ifndef __DRIVERS_DDS_HPP__
#define __DRIVERS_DDS_HPP__

#include <context.hpp>

#include <array>

class ClockGenerator;

class Dds
{
  public:
    Dds(Context& ctx_);
    void set_dds_freq(uint32_t channel, double freq_hz);

    auto get_dds_freq(uint32_t channel) {
        return dds_freq[channel];
    }

  private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    ClockGenerator& clk_gen;

    std::array<double, 2> dds_freq = {{0.0, 0.0}};
};

#endif // __DRIVERS_DDS_HPP__
