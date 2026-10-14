#include "../measurements.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <iostream>
int main()
{
    assert(bist::utc_timestamp(0, 3117000) == "1970-01-01T00:00:00.003117Z");
    assert(bist::quote("\"\\\n\t") == "\"\\\"\\\\\\u000a\\u0009\"");
    assert(bist::number(std::numeric_limits<double>::quiet_NaN()) == "null");
    assert(bist::distribution({}) == "null");
    assert(bist::distribution({3, 1, 2}).find("\"p50\":2") != std::string::npos);
    assert(bist::distribution({1, 2}).find("\"p50\":1") != std::string::npos);
    assert(bist::distribution({1, 2}).find("\"p95\":null") != std::string::npos);
    bist::LatencyHistogram histogram, tail;
    for (unsigned i = 0; i < 8192; ++i) histogram.add(10);
    for (unsigned i = 0; i < 1000; ++i) tail.add(1000);
    histogram.merge(tail);
    histogram.add(std::numeric_limits<double>::quiet_NaN());
    assert(histogram.count == 9192 && histogram.invalid == 1);
    assert(histogram.minimum == 10 && histogram.maximum == 1000);
    assert(std::abs(histogram.percentile(.99) / 1000 - 1) < .0078125);
    assert(histogram.percentile(.5) < 11);
    assert(tail.json().find("\"p99\":1000") != std::string::npos);
    bist::LatencyHistogram small;
    small.add(2); small.add(100);
    assert(small.json().find("\"p95\":null") != std::string::npos);
    assert(bist::adc_signed(0x3fff, 0) == -1);
    assert(bist::adc_signed(0x20000000, 1) == -8192);
    std::vector<int16_t> data(16384);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = int16_t(53 + ((i % 4) == 0 ? 4096 : (i % 4) == 2 ? -4096 : 0));
    const auto tone = bist::fit_tone(data);
    assert(std::abs(tone.mean - 53) < 1e-9);
    assert(std::abs(tone.amplitude - 4096) < 1e-9);
    assert(tone.residual_rms < 1e-9 && tone.clipping < 1e-9);
    // Pattern depends on high-order indices and endpoint; catch swapped links,
    // stale buffers, lost bytes and descriptor reuse with the old generation.
    assert(bist::pattern(1, 1) != bist::pattern(1, 2));
    assert(bist::serial_pattern(0, 0) != bist::serial_pattern(0, 1));
    assert(bist::serial_pattern(0, 0) != bist::serial_pattern(256, 0));
    std::cout << "measurement helpers PASS\n";
}
