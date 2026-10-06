#pragma once
#include <array>
#include <cstdint>
enum MemID { dma_id, hp_id };
namespace mem { constexpr MemID dma=dma_id, axi_hp0=hp_id; }
namespace hw {
template<MemID id> struct Memory {
    std::array<uint32_t, 64> words{};
    unsigned writes=0;
    template<uint32_t offset> uint32_t read() { return words[offset/4]; }
    template<uint32_t offset> void write(uint32_t value) { words[offset/4]=value; ++writes; }
    template<uint32_t offset, unsigned bit> void set_bit() {
        ++writes;
        if constexpr (id==dma_id && offset==0x30 && bit==2) words[offset/4]=0; // completed reset
        else words[offset/4] |= 1u << bit;
    }
    template<uint32_t offset, unsigned bit> bool read_bit() { return (read<offset>() >> bit) & 1u; }
};
template<MemID id> Memory<id>& get_memory() { static Memory<id> value; return value; }
}
