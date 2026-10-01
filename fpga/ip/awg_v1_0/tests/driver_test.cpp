#include "server/drivers/dds/phase-modulator.hpp"
#include <array>
#include <cassert>
#include <vector>

struct Memory {
    std::array<uint32_t, 32> registers{};
    std::vector<std::pair<uint32_t, uint32_t>> writes;
    template<class T> T read_reg(uint32_t offset) { return registers.at(offset / 4); }
    template<class T> void write_reg(uint32_t offset, T value) {
        registers.at(offset / 4) = value;
        writes.emplace_back(offset, value);
    }
};

int main() {
    Memory memory;
    memory.registers[0] = 0x504d0001;
    memory.registers[1] = 1023;
    memory.registers[2] = 0x1f0e1830;
    dds_pm::Controller<Memory> controller(memory);
    assert(controller.phase_width() == 48);
    dds_pm::Settings settings;
    settings.carrier_increment = 0x123456789abc;
    settings.carrier_phase = 1;
    settings.modulation_increment = 0xabcdef012345;
    settings.modulation_phase = 0xffffffffffff;
    settings.deviation = uint64_t{1} << 48;
    settings.duty = uint64_t{1} << 47;
    settings.output_enabled = settings.pm_enabled = true;
    settings.waveform = dds_pm::Waveform::triangle;
    assert(controller.configure(settings, true, true));
    assert(memory.registers[0x20/4] == 0x56789abc);
    assert(memory.registers[0x24/4] == 0x1234);
    assert(memory.registers[0x44/4] == 0x10000); // Full-turn depth survives.
    assert(memory.writes.back().first == 0x10 && memory.writes.back().second == 7);
    assert(memory.registers[0x54/4] == 0x303);
    settings.deviation = 1;
    assert(controller.configure(settings));
    assert(memory.registers[0x40/4] == 1 && memory.registers[0x44/4] == 0);
    assert(memory.writes.back().second == 1);
    auto count = memory.writes.size();
    settings.deviation = (uint64_t{1} << 48) + 1;
    assert(!controller.configure(settings));
    assert(memory.writes.size() == count);
    settings.deviation = 0;
    settings.waveform = dds_pm::Waveform::prbs;
    settings.seed = 0x80000000;
    assert(!controller.configure(settings));
    memory.registers[1] = 2;
    dds_pm::Controller<Memory> small(memory);
    settings.waveform = dds_pm::Waveform::triangle;
    settings.seed = 1;
    assert(!small.configure(settings));
    settings.pm_enabled = false;
    assert(small.configure(settings)); // Tone is usable without any PM sources.
}
