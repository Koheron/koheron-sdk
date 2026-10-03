/// DDS driver
///
/// (c) Koheron

#ifndef __DRIVERS_DDS_HPP__
#define __DRIVERS_DDS_HPP__

#include <array>
#include <cstdint>
#include <mutex>


class Dds
{
  public:
    Dds();
    void set_dds_freq(uint32_t channel, double freq_hz);

    auto get_dds_freq(uint32_t channel) {
        std::lock_guard lock(mutex);
        return channel < dds_freq.size() ? dds_freq[channel] : 0.0;
    }

  private:
    std::mutex mutex;
    std::array<double, 2> dds_freq = {{0.0, 0.0}};
};

#endif // __DRIVERS_DDS_HPP__
