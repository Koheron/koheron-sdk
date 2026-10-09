#pragma once
#include <cstdint>

// Register-only substitute: the production drivers are included unmodified.
namespace mem { inline constexpr unsigned xadc = 0; }
namespace hw {
template<unsigned ID> struct Memory {
    uint32_t registers[256]{};
    template<uint32_t Offset> uint32_t read() const {
        static_assert(Offset / 4 < 256);
        return static_cast<const volatile uint32_t*>(registers)[Offset / 4];
    }
    uint32_t read_reg(uint32_t offset) const { return registers[offset / 4]; }
    template<uint32_t Offset> void write(uint32_t value) { registers[Offset / 4] = value; }
    template<uint32_t Offset, uint32_t Mask> void write_mask(uint32_t value) {
        registers[Offset / 4] = (registers[Offset / 4] & ~Mask) | (value & Mask);
    }
};
inline Memory<mem::xadc> xadc_fixture;
template<unsigned ID> Memory<ID>& get_memory() {
    static_assert(ID == mem::xadc);
    return xadc_fixture;
}
}
