"""Exercise the shared ALPHA250 RPC driver with simulated MMIO; no board access."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


class PhaseModulatorIntegrationTest(unittest.TestCase):
    def test_sample_clocks_read_only_discovery_and_independent_references(self):
        root = Path(__file__).resolve().parents[4]
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            files = {
                "server/hardware/memory_manager.hpp": r'''
#pragma once
#include <array>
#include <cstdint>
#include <utility>
#include <vector>
constexpr int ERROR = 3;
template<int, class... Args> void logf(const char*, Args&&...) {}
template<class... Args> void logf(const char*, Args&&...) {}
template<int> void log(const char*) {}
namespace prm { constexpr uint32_t adc_clk = TEST_SAMPLE_RATE; }
namespace mem { enum {awg, control}; }
namespace reg { constexpr uint32_t phase_incr0 = 16; }
namespace hw {
template<int id> class Memory {
public:
    std::array<uint32_t, 2048> words{};
    std::vector<std::pair<uint32_t, uint64_t>> writes;
    Memory() {
        if constexpr (id == mem::awg) {
            for (unsigned bank : {0u, 1024u}) {
                words[bank] = 0x504d0001;
                words[bank+1] = 1023;
                words[bank+2] = 0x1f0e1830;
                words[bank+6] = 16;
                words[bank+7] = 2;
                words[bank+0x4c/4] = 0x8000;
                words[bank+0x50/4] = 1;
            }
        }
    }
    template<class T> T read_reg(uint32_t offset) {
        return static_cast<T>(words.at(offset / 4));
    }
    template<class T> void write_reg(uint32_t offset, T value) {
        words.at(offset / 4) = static_cast<uint32_t>(value);
        if constexpr (sizeof(T) == 8) words.at(offset/4+1) = static_cast<uint32_t>(value >> 32);
        writes.emplace_back(offset, value);
    }
};
template<int id> Memory<id>& get_memory() { static Memory<id> value; return value; }
}
''',
                "boards/alpha250/drivers/clock-generator.hpp": r'''
#pragma once
#include <cstdint>
class ClockGenerator {
public:
    uint32_t selection = 99;
    void set_sampling_frequency(uint32_t value) { selection = value; }
    double get_dac_sampling_freq() { return TEST_SAMPLE_RATE; }
};
namespace rt {
template<class T> T& get_driver() { static T value; return value; }
}
''',
                "server/runtime/driver_manager.hpp": "#pragma once\n",
                "server/runtime/syslog.hpp": "#pragma once\n",
                "test.cpp": r'''
#include "server/hardware/memory_manager.hpp"
#include "boards/alpha250/drivers/clock-generator.hpp"
// Include the real RPC declaration, bypassing only board IO dependencies.
#include PHASE_MODULATOR_HEADER
#include "examples/alpha250/phase-noise-analyzer/dds.hpp"
#include <cassert>
#include <cmath>
#include <string>
int main() {
    PhaseModulator pm;
    auto& clock = rt::get_driver<ClockGenerator>();
    constexpr uint32_t expected = TEST_SAMPLE_RATE == 200000000 ? 0 :
        TEST_SAMPLE_RATE == 250000000 ? 1 : TEST_SAMPLE_RATE == 100000000 ? 2 : 3;
    assert(clock.selection == expected);
    assert(pm.get_sample_rate() == TEST_SAMPLE_RATE);
    assert(pm.get_channel_count() == 2 && pm.get_phase_width(0) == 48);
    assert(pm.get_initialization_error().empty());
    auto& awg = hw::get_memory<mem::awg>();
    assert(awg.writes.empty()); // Discovery cannot configure or enable a DAC.
    auto untouched = pm.get_settings_words(1);
    assert(std::get<0>(untouched) == 0 && !std::get<9>(untouched));

    assert(pm.configure(0, 10e6, 17, 10e3, 0, 1, .5, 1, 0, true, true, false).empty());
    const auto settings = pm.get_settings_words(0);
    const uint64_t turn = uint64_t{1} << 48;
    assert(std::get<1>(settings) == static_cast<uint64_t>(std::round(10e6L / TEST_SAMPLE_RATE * turn)));
    assert(std::get<9>(settings) && std::get<10>(settings));
    assert(pm.get_settings_words(1) == untouched);

    if constexpr (TEST_SAMPLE_RATE == 200000000) {
        Dds lo;
        lo.set_dds_freq(0, 12e6);
        assert(pm.get_settings_words(0) == settings);
        const auto references = hw::get_memory<mem::control>().words;
        assert(pm.set_modulation_frequency(0, 12e3).empty());
        assert(hw::get_memory<mem::control>().words == references);
        assert(std::get<1>(pm.get_settings_words(0)) == std::get<1>(settings));
        assert(lo.get_dds_freq(0) == 12e6);
    }
    const auto count = awg.writes.size();
    assert(!pm.set_carrier_frequency(0, TEST_SAMPLE_RATE / 2.0).empty());
    assert(awg.writes.size() == count);
    assert(pm.mute(0).empty());
    assert(!std::get<9>(pm.get_settings_words(0)));
    assert(std::get<1>(pm.get_settings_words(0)) == std::get<1>(settings));
}
''',
            }
            for name, contents in files.items():
                path = temp / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(contents)
            compiler = shlex.split(os.environ.get("CXX", "g++"))
            for rate in (200_000_000, 250_000_000, 100_000_000, 240_000_000):
                with self.subTest(sample_rate=rate):
                    executable = temp / f"test-{rate}"
                    subprocess.run(compiler + [
                        "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-fno-exceptions",
                        f"-DTEST_SAMPLE_RATE={rate}",
                        f'-DPHASE_MODULATOR_HEADER="{root}/boards/alpha250/drivers/phase-modulator.hpp"',
                        "-I", str(temp), "-I", str(root), str(temp / "test.cpp"),
                        str(root / "examples/alpha250/phase-noise-analyzer/dds.cpp"),
                        "-o", str(executable),
                    ], check=True)
                    subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
