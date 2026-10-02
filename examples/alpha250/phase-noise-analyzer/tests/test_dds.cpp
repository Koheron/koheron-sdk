#include "../dds.hpp"
#include "server/hardware/memory_manager.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <thread>
#include <iostream>

int main() {
    Dds dds;
    auto& ctl = hw::get_memory<mem::control>();
    constexpr double lsb = 200e6 / (uint64_t{1} << 48);
    for (double freq : {0., 10e6 + .637, 100e6, -1., 200e6}) {
        for (uint32_t channel : {0u, 1u}) {
            dds.set_dds_freq(channel, freq);
            const auto word = uint64_t(ctl.words[2 * channel].load()) |
                              uint64_t(ctl.words[2 * channel + 1].load()) << 32;
            const double expected = std::clamp(freq, 0., 100e6);
            assert(word == uint64_t(std::llround(expected / lsb)));
            assert(dds.get_dds_freq(channel) == word * lsb);
            assert(std::abs(dds.get_dds_freq(channel) - expected) <= lsb / 2);
        }
    }
    const auto writes = ctl.writes.load();
    for (double freq : {std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN()})
        dds.set_dds_freq(0, freq);
    dds.set_dds_freq(2, 1e6);
    assert(ctl.writes.load() == writes);
    assert(dds.get_dds_freq(2) == 0.);
    std::thread writer([&] {
        for (int i = 0; i < 10000; ++i) dds.set_dds_freq(i % 2, 10e6 + i * .001);
    });
    std::thread reader([&] {
        for (int i = 0; i < 10000; ++i) {
            const auto value = dds.get_dds_freq(i % 2);
            assert(std::isfinite(value) && value >= 0 && value <= 100e6);
        }
    });
    writer.join(); reader.join();
    std::cout << "DDS rounding, ADC clock, validation and concurrent access checks passed\n";
}
