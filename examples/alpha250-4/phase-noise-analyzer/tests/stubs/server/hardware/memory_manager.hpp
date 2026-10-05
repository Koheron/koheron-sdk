#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>

using MemID = int;
namespace mem { enum {ram, mux, control, dma, axi_hp0, status}; }
namespace prm {
constexpr uint32_t cic_decimation_rate_default=20, cic_decimation_rate_min=4,
    cic_decimation_rate_max=8192, cic_n_stages=6, cic_differential_delay=1;
}
namespace reg {
constexpr uint32_t phase_incr0=0, cordic=32, cic_rate=36, phase_precision=40,
    scaling0=44, scaling1=48, scaling2=52, scaling3=56, acquisition_run=60;
constexpr uint32_t demod0=0, demod2=8, fifo_wr_data_count0=16,
    fifo_wr_data_count1=20, sample_gap=24;
}
namespace hw {
inline std::recursive_mutex simulated_bus;
inline std::atomic<uint32_t> simulated_epoch{0};
inline std::atomic<uint32_t> selected_input{0};
inline std::atomic<uint32_t> injected_x_status{0}, injected_y_status{0}, completed_packet_status{0};
template<int id> class Memory {
  public:
    static constexpr uint32_t phys_addr = 0;
    static constexpr uint32_t size = 128 * 1024 * 1024;
    std::array<std::atomic<uint32_t>, 2048> registers{};
    std::vector<int32_t> data;
    Memory() { if constexpr (id == mem::ram) data.resize(128 * 1024 * 1024 / sizeof(int32_t)); }
    template<typename T, uint32_t N> auto& read_reg_array(uint32_t offset) {
        return *reinterpret_cast<std::array<T, N>*>(reinterpret_cast<char*>(data.data()) + offset);
    }
    template<uint32_t offset, typename T=uint32_t> T read() {
        if constexpr (id == mem::mux && offset == 4) return completed_packet_status.load();
        return registers[offset / 4];
    }
    template<uint32_t offset, uint32_t bit> bool read_bit() { return (read<offset>() >> bit) & 1; }
    template<uint32_t offset, uint32_t bit> void set_bit() { registers[offset / 4] |= 1u << bit; }
    template<uint32_t offset, uint32_t bit> void clear_bit() { registers[offset / 4] &= ~(1u << bit); }
    template<typename T=uint32_t> T read_reg(uint32_t offset) {
        if constexpr(id==mem::ram) return T(data[offset/4]);
        else return T(registers[offset/4].load());
    }
    template<typename T> void write_reg(uint32_t offset, T value) {
        if constexpr(id==mem::ram) data[offset/4]=int32_t(value);
        else {
            registers[offset/4].store(uint32_t(value));
            if constexpr(sizeof(T)==8) registers[offset/4+1].store(uint32_t(value >> 32));
        }
    }
    template<uint32_t offset> void write(uint32_t value) {
        std::lock_guard lock(simulated_bus);
        if constexpr (id == mem::mux) {
            selected_input.store(value & 1);
            registers[offset/4]=value;
            if(!(value&2)) { completed_packet_status.store(0); ++simulated_epoch; }
        } else if constexpr(id==mem::dma && offset==0x30) {
            registers[offset/4]=(value&4) ? 0u : value;
            registers[0x34/4]=(value&4) ? 1u : 0u;
        } else registers[offset / 4] = value;
    }
};
template<int id> Memory<id>& get_memory() { static Memory<id> memory; return memory; }
}
namespace rt {
template<typename Driver> Driver& get_driver() { static Driver driver; return driver; }
}
