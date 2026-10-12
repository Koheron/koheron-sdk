#pragma once
#include <array>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

namespace mem {
enum { control, dma, ram_mm2s, ram_s2mm, axi_hp0, axi_hp2, ocm_mm2s, ocm_s2mm, sclr };
constexpr uint32_t ocm_mm2s_addr = 0xffff0000, ocm_s2mm_addr = 0xffff8000;
constexpr uint32_t ram_mm2s_addr = 0x18000000, ram_s2mm_addr = 0x1c000000;
}
namespace reg {
constexpr uint32_t reset = 0, channel_select = 4, pkt_count = 8, trig = 12;
}
namespace hw {
// Substitute only MMIO; include the real DMA driver unchanged.
template<unsigned ID> struct Memory {
    static constexpr uint32_t size = (ID == mem::ram_mm2s || ID == mem::ram_s2mm)
        ? 64 * 1024 * 1024 : 64 * 1024;
    std::vector<uint32_t> words = std::vector<uint32_t>(size / 4, 0xa5a5a5a5);
    size_t writes = 0;
    void write_reg(uint32_t offset, uint32_t value) {
        if (offset % 4 != 0 || offset / 4 >= words.size()) std::abort();
        words[offset / 4] = value;
        ++writes;
    }
    template<uint32_t Offset> void write(uint32_t value) { write_reg(Offset, value); }
    template<uint32_t Offset> uint32_t read() const { return words.at(Offset / 4); }
    template<uint32_t Offset, unsigned Bit> void set_bit() { write<Offset>(read<Offset>() | (1u << Bit)); }
    template<uint32_t Offset, unsigned Bit> void clear_bit() { write<Offset>(read<Offset>() & ~(1u << Bit)); }
    template<uint32_t Offset, unsigned Bit> bool read_bit() const { return (read<Offset>() >> Bit) & 1u; }
    template<class T> std::span<T> read_span(size_t count) { return {words.data(), count}; }
    template<class T, size_t N> std::array<T, N> read_array() { return {}; }
};
template<unsigned ID> Memory<ID>& get_memory() {
    static Memory<ID> memory;
    return memory;
}
}
