#include "./dds.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/hardware/memory_manager.hpp"
#include "boards/alpha250/drivers/clock-generator.hpp"

#include <limits>
#include <cmath>

Dds::Dds()
: clk_gen(rt::get_driver<ClockGenerator>())
{
    clk_gen.set_sampling_frequency(0);
}

void Dds::set_dds_freq(uint32_t channel, double freq_hz) {
    std::lock_guard lock(mutex);
    if (channel >= 2) {
        log<ERROR>("Dds::set_dds_freq invalid channel\n");
        return;
    }

    if (!std::isfinite(freq_hz)) {
        log<ERROR>("Dds::set_dds_freq Frequency is not finite\n");
        return;
    }

    double fs_adc = clk_gen.get_adc_sampling_freq();

    if (freq_hz > fs_adc / 2.0) {
        freq_hz = fs_adc / 2.0;
    }

    if (freq_hz < 0.0) {
        freq_hz = 0.0;
    }

    double factor = (uint64_t(1) << 48) / fs_adc;

    auto& ctl= hw::get_memory<mem::control>();
    const auto phase_incr = static_cast<uint64_t>(std::llround(factor * freq_hz));
    ctl.write_reg<uint64_t>(reg::phase_incr0 + 8 * channel, phase_incr);
    dds_freq[channel] = static_cast<double>(phase_incr) / factor;

    logf("fs: {}, channel {}, ref. frequency set to {}\n", fs_adc, channel, freq_hz);
}
