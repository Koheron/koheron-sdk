#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace mem {
enum {control, status, dma0, dma1, ram0, ram1, ocm0, ocm1, axi_hp0, axi_hp2, sclr};
constexpr uint32_t ram0_addr = 0x18000000, ram1_addr = 0x1c000000;
constexpr uint32_t ram0_range = 64 * 1024 * 1024, ram1_range = ram0_range;
constexpr uint32_t ocm0_addr = 0xffff0000, ocm1_addr = 0xffff4000;
constexpr uint32_t ocm0_range = 16384, ocm1_range = 16384;
}
namespace reg {
constexpr uint32_t acq_reset = 16, trigger = 20, sample_count = 24, test_pattern = 28;
constexpr uint32_t capture_status = 0, captured_samples = 4, pll_locked = 8;
}
namespace hw {
template<int Id> class Memory {
  public:
    std::array<uint32_t, 16384> registers{};
    std::vector<uint32_t> data;
    bool reset_stuck = false;
    Memory() {
        if constexpr (Id == mem::ram0 || Id == mem::ram1) data.resize(131072);
        if constexpr (Id == mem::status) registers[reg::pll_locked / 4] = 1;
    }
    template<uint32_t Offset> uint32_t read() { return registers[Offset / 4]; }
    template<uint32_t Offset> void write(uint32_t value) {
        if constexpr ((Id == mem::dma0 || Id == mem::dma1) && Offset == 0x30) {
            registers[Offset / 4] = ((value & 4) && !reset_stuck) ? 0 : value;
            if (value & 4) registers[0x34 / 4] = 1;
        } else registers[Offset / 4] = value;
    }
    template<uint32_t Offset, uint32_t Bit> void clear_bit() { registers[Offset / 4] &= ~(1U << Bit); }
    uint32_t read_reg(uint32_t offset) { return registers[offset / 4]; }
    void write_reg(uint32_t offset, uint32_t value) { registers[offset / 4] = value; }
    template<typename T> std::span<const T> read_span(uint32_t count) {
        static_assert(sizeof(T) == 4);
        assert(count <= data.size());
        return {reinterpret_cast<const T*>(data.data()), count};
    }
};
template<int Id> Memory<Id>& get_memory() { static Memory<Id> memory; return memory; }
}
