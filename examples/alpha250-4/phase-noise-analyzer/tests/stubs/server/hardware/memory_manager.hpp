#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

using MemID = int;
namespace mem { enum {ram, mux, control, dma, axi_hp0}; }
namespace reg { constexpr uint32_t phase_incr0 = 0; }
namespace hw {
inline std::atomic<uint32_t> selected_input{0};
inline std::atomic<uint32_t> injected_x_status{0}, injected_y_status{0}, completed_packet_status{0};
template<int id> class Memory {
  public:
    static constexpr uint32_t phys_addr = 0;
    static constexpr uint32_t size = 128 * 1024 * 1024;
    std::array<uint32_t, 64> registers{};
    std::vector<int32_t> data;
    Memory() { if constexpr (id == mem::ram) data.resize(128 * 1024 * 1024 / sizeof(int32_t)); }
    template<typename T, uint32_t N> auto& read_reg_array(uint32_t offset) {
        return *reinterpret_cast<std::array<T, N>*>(reinterpret_cast<char*>(data.data()) + offset);
    }
    template<uint32_t offset> uint32_t read() {
        if constexpr (id == mem::mux && offset == 4) return completed_packet_status.load();
        return registers[offset / 4];
    }
    template<uint32_t offset, uint32_t bit> bool read_bit() { return (read<offset>() >> bit) & 1; }
    template<uint32_t offset, uint32_t bit> void set_bit() { registers[offset / 4] |= 1u << bit; }
    template<typename T> void write_reg(uint32_t, T) {}
    template<uint32_t offset> void write(uint32_t value) {
        if constexpr (id == mem::mux) selected_input.store(value & 1);
        else registers[offset / 4] = value;
    }
};
template<int id> Memory<id>& get_memory() { static Memory<id> memory; return memory; }
}
namespace rt {
template<typename Driver> Driver& get_driver() { static Driver driver; return driver; }
}
