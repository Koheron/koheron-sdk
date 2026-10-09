"""Exercise the shared ALPHA250 and Red Pitaya RPC drivers with simulated MMIO; no board access."""

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
#include <mutex>
namespace clock_cfg { inline std::recursive_mutex sampling_mutex; }
class ClockGenerator {
public:
    uint32_t selection = 99;
    double rate = TEST_SAMPLE_RATE;
    void set_sampling_frequency(uint32_t value) { selection = value; rate = value == 0 ? 200000000 : 250000000; }
    double get_dac_sampling_freq() { return rate; }
    double get_adc_sampling_freq() { return rate; }
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
#ifndef TEST_RED_PITAYA
struct Alpha250PhaseNoiseBoard {
    static bool compatible(PhaseModulator& pm, uint32_t rate) { return pm.sample_rate_compatible(rate); }
    static bool change(PhaseModulator& pm, uint32_t rate) {
        std::lock_guard lock(clock_cfg::sampling_mutex);
        return pm.change_sample_rate(rate);
    }
};
#endif
int main() {
    PhaseModulator pm;
    auto& clock = rt::get_driver<ClockGenerator>();
    assert(clock.selection == 99); // Discovery never selects a host clock.
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
        assert(std::abs(lo.get_dds_freq(0) - 12e6) < 1e-6);
    }
    const auto count = awg.writes.size();
    assert(!pm.set_carrier_frequency(0, TEST_SAMPLE_RATE / 2.0).empty());
    assert(awg.writes.size() == count);
    assert(pm.mute(0).empty());
    assert(!std::get<9>(pm.get_settings_words(0)));
    assert(std::get<1>(pm.get_settings_words(0)) == std::get<1>(settings));
#ifndef TEST_RED_PITAYA
    clock.rate = TEST_SAMPLE_RATE == 200000000 ? 250000000 : 200000000;
    assert(pm.get_sample_rate() == clock.rate);
    const auto other = pm.get_settings_words(1);
    assert(pm.set_carrier_frequency(0, 12e6).empty());
    assert(std::get<1>(pm.get_settings_words(0)) ==
        static_cast<uint64_t>(std::round(12e6L / clock.rate * turn)));
    assert(pm.set_modulation_frequency(0, 12e3).empty());
    assert(std::get<3>(pm.get_settings_words(0)) ==
        static_cast<uint64_t>(std::round(12e3L / clock.rate * turn)));
    assert(pm.get_settings_words(1) == other);
    const auto after = awg.writes.size();
    assert(!pm.set_carrier_frequency(0, clock.rate / 2).empty());
    assert(awg.writes.size() == after);
    assert(pm.set_output_enabled(0, true).empty());
    assert(pm.configure(1, 15e6, 23, 17e3, 11, .87, .4, 123, 0, false, true, false).empty());
    for (uint32_t rate : {200000000u, 250000000u, 200000000u, 250000000u}) {
        const auto saved0 = pm.get_settings_words(0), saved1 = pm.get_settings_words(1);
        assert(Alpha250PhaseNoiseBoard::change(pm, rate));
        assert(pm.get_sample_rate() == rate);
        const auto changed0 = pm.get_settings_words(0), changed1 = pm.get_settings_words(1);
        assert(std::abs(double(std::get<1>(changed0)) / turn * rate - 12e6) < 1e-6);
        assert(std::abs(double(std::get<3>(changed0)) / turn * rate - 12e3) < 1e-6);
        assert(std::abs(double(std::get<1>(changed1)) / turn * rate - 15e6) < 1e-6);
        assert(std::abs(double(std::get<3>(changed1)) / turn * rate - 17e3) < 1e-6);
        for (const auto& pair : {std::pair{saved0, changed0}, std::pair{saved1, changed1}}) {
            assert(std::get<2>(pair.first) == std::get<2>(pair.second));
            assert(std::get<4>(pair.first) == std::get<4>(pair.second));
            assert(std::get<5>(pair.first) == std::get<5>(pair.second));
            assert(std::get<6>(pair.first) == std::get<6>(pair.second));
            assert(std::get<7>(pair.first) == std::get<7>(pair.second));
            assert(std::get<8>(pair.first) == std::get<8>(pair.second));
            assert(std::get<9>(pair.first) == std::get<9>(pair.second));
            assert(std::get<10>(pair.first) == std::get<10>(pair.second));
        }
    }
    assert(pm.set_carrier_frequency(1, 110e6).empty());
    const auto rejected = pm.get_settings_words(1);
    const auto writes_before_rejection = awg.writes.size();
    assert(!Alpha250PhaseNoiseBoard::compatible(pm, 200000000));
    assert(!Alpha250PhaseNoiseBoard::change(pm, 200000000));
    assert(pm.get_sample_rate() == 250000000);
    assert(pm.get_settings_words(1) == rejected && awg.writes.size() == writes_before_rejection);
#endif
}
''',
            }
            for name, contents in files.items():
                path = temp / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(contents)
            compiler = shlex.split(os.environ.get("CXX", "g++"))
            configurations = [("alpha250", rate) for rate in (200_000_000, 250_000_000, 100_000_000, 240_000_000)]
            configurations.append(("red-pitaya", 125_000_000))
            for board, rate in configurations:
                with self.subTest(board=board, sample_rate=rate):
                    executable = temp / f"test-{rate}"
                    subprocess.run(compiler + [
                        "-std=c++23", "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-fno-exceptions",
                        f"-DTEST_SAMPLE_RATE={rate}",
                        f'-DPHASE_MODULATOR_HEADER="{root}/boards/{board}/drivers/phase-modulator.hpp"',
                        *(['-DTEST_RED_PITAYA'] if board == 'red-pitaya' else []),
                        "-I", str(temp), "-I", str(root), str(temp / "test.cpp"),
                        str(root / "examples/alpha250/phase-noise-analyzer/dds.cpp"),
                        "-o", str(executable),
                    ], check=True)
                    subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
