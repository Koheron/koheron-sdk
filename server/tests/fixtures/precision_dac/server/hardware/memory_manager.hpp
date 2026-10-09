#pragma once
#include <array>
#include <cstdint>
#include "server/runtime/services.hpp"
namespace mem { inline constexpr unsigned control = 0; }
namespace reg {
inline constexpr uint32_t precision_dac_ctl = 0;
inline constexpr uint32_t precision_dac_data0 = 4;
inline constexpr uint32_t precision_dac_data1 = 8;
}
namespace hw {
template<unsigned ID> struct Memory {
    volatile uint32_t registers[3]{};
    std::array<unsigned, 3> writes{};
    template<uint32_t Offset> void write(uint32_t value) {
        registers[Offset / 4] = value;
        ++writes[Offset / 4];
    }
};
inline Memory<mem::control> control_fixture;
class MemoryManager {
  public:
    template<unsigned ID> Memory<ID>& get() {
        static_assert(ID == mem::control);
        return control_fixture;
    }
};
template<unsigned ID> Memory<ID>& get_memory() {
    return services::require<MemoryManager>().get<ID>();
}
}
