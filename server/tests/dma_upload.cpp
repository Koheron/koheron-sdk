#include "examples/alpha250/adc-dac-dma/adc-dac-dma.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <string_view>

void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}

int main(int argc, char** argv) {
    check(argc == 2, "case required");
    const std::string_view test{argv[1]};
    auto driver = std::make_unique<AdcDacDma>();
    auto& ram = hw::get_memory<mem::ram_mm2s>();
    constexpr uint32_t capacity = hw::Memory<mem::ram_mm2s>::size / 4;
    if (test == "two-descriptors") {
        std::vector<uint32_t> waveform(2 * n_pts);
        for (size_t i = 0; i < waveform.size(); ++i) waveform[i] = 0x12340000u + i * 65537u;
        for (uint32_t offset = 0; offset < waveform.size(); offset += 32768) {
            const std::vector<uint32_t> chunk(waveform.begin() + offset, waveform.begin() + offset + 32768);
            check(driver->set_dac_data_chunk(offset, chunk), "chunk rejected");
        }
        check(std::equal(waveform.begin(), waveform.end(), ram.words.begin()), "waveform order or offset incorrect");
        check(ram.words[waveform.size()] == 0xa5a5a5a5, "write beyond waveform");
    } else if (test == "bounds") {
        check(driver->set_dac_data_chunk(capacity - 1, {42}), "last word rejected");
        check(ram.words.back() == 42, "last word not written");
        ram.writes = 0;
        check(driver->set_dac_data_chunk(capacity, {}), "empty end rejected");
        check(!driver->set_dac_data_chunk(capacity - 1, {1, 2}), "cross-boundary write accepted");
        check(!driver->set_dac_data_chunk(capacity, {1}), "out-of-range write accepted");
        check(!driver->set_dac_data_chunk(capacity + 1, {}), "out-of-range empty write accepted");
        check(!driver->set_dac_data_chunk(std::numeric_limits<uint32_t>::max(), {1, 2}), "offset overflow accepted");
        check(ram.writes == 0 && ram.words.back() == 42, "invalid chunk partially written");
    } else if (test == "legacy") {
        const std::vector<uint32_t> waveform(n_pts, 0x12345678);
        driver->set_dac_data(waveform);
        check(std::equal(waveform.begin(), waveform.end(), ram.words.begin()), "legacy upload changed");
        check(ram.words[n_pts] == 0xa5a5a5a5, "legacy upload overran");
        ram.writes = 0;
        driver->set_dac_data(std::vector<uint32_t>(capacity + 1, 99));
        check(ram.writes == 0, "oversized legacy write reached RAM");
    } else { check(false, "unknown case"); }
}
