#include "./dds.hpp"

#include "server/runtime/services.hpp"
#include "server/runtime/syslog.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/hardware/memory_manager.hpp"
#include "boards/alpha250/drivers/clock-generator.hpp"

#include <limits>
#include <cmath>

using services::require;

Dds::Dds()
: clk_gen(require<rt::DriverManager>().get<ClockGenerator>())
{
    clk_gen.set_sampling_frequency(0);
}

void Dds::set_dds_freq(uint32_t channel, double freq_hz) {
    if (channel >= 2) {
        log<ERROR>("FFT::set_dds_freq invalid channel\n");
        return;
    }

    if (std::isnan(freq_hz)) {
        log<ERROR>("FFT::set_dds_freq Frequency is NaN\n");
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

    auto& ctl= require<hw::MemoryManager>().get<mem::control>();
    ctl.write_reg<uint64_t>(reg::phase_incr0 + 8 * channel, uint64_t(factor * freq_hz));
    dds_freq[channel] = freq_hz;

    logf("fs: {}, channel {}, ref. frequency set to {}\n", fs_adc, channel, freq_hz);
}