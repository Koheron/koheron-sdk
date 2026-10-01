#pragma once
#include <array>
#include <atomic>
#include <cstdint>
namespace prm {
inline constexpr unsigned fft_size = TEST_FFT_SIZE, adc_width = TEST_ADC_WIDTH;
inline constexpr unsigned adc_clk = TEST_ADC_RATE, n_adc = 2, n_cycles = 1023;
}
namespace mem { enum {control, status, demod, psd}; }
namespace reg { enum {psd_valid, psd_input_sel, ctl_fft, adc0, adc1, cycle_index, phase_incr0 = 16}; }
namespace fake {
inline std::atomic<bool> advance{true};
inline std::atomic<uint32_t> cycle{0};
inline std::atomic<float> spectrum{3};
inline std::array<uint32_t, prm::fft_size> window{};
inline std::array<std::atomic<uint32_t>, 32> registers{};
}
namespace hw {
template <unsigned Id> struct Memory {
    template <unsigned Reg, unsigned Bit> void set_bit() { fake::registers[Reg] |= 1U << Bit; }
    template <unsigned Reg> void write(uint32_t value) { fake::registers[Reg] = value; }
    void write_reg(unsigned reg, uint32_t value) { fake::registers[reg] = value; }
    template <unsigned Reg> uint32_t read() {
        if constexpr (Reg == reg::cycle_index) {
            return fake::advance && (fake::cycle.fetch_add(1) % 2 == 0) ? prm::n_cycles : 0;
        } else { return fake::registers[Reg]; }
    }
    template <typename T> void write_array(const T& values) { fake::window = values; }
    template <typename T, unsigned N> std::array<T, N> read_array() {
        std::array<T, N> values;
        values.fill(fake::spectrum.load());
        return values;
    }
};
template <unsigned Id> Memory<Id>& get_memory() { static Memory<Id> value; return value; }
}
