#include "../dds.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>

int main() {
    Dds dds;
    constexpr double requested = 10e6 + 0.637;
    dds.set_dds_freq(1, requested, false);
    assert(std::abs(dds.get_dds_freq(1) - requested) < 1e-6);
    std::thread writer([&] {
        for (int i = 0; i < 10000; ++i) dds.set_dds_freq(1, requested + i / 10000., false);
    });
    for (int i = 0; i < 10000; ++i) {
        const double frequency = dds.get_dds_freq(1);
        assert(std::isfinite(frequency) && frequency > requested - 1e-6 && frequency < requested + 1.);
    }
    writer.join();
    const double previous = dds.get_dds_freq(1);
    dds.set_dds_freq(1, NAN, false);
    dds.set_dds_freq(1, INFINITY, false);
    assert(dds.get_dds_freq(1) == previous);
    std::cout << "DDS precision, concurrent access and nonfinite-input tests passed\n";
}
