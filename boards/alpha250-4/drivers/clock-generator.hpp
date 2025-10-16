#ifndef __ALPHA250_4_DRIVERS_CLOCK_GENERATOR_HPP__
#define __ALPHA250_4_DRIVERS_CLOCK_GENERATOR_HPP__

#include "server/hardware/memory_manager.hpp"

#include <array>
#include <cstdint>
#include <string_view>

using namespace std::string_view_literals;

namespace clock_cfg {
    // Input clock selection
    constexpr uint32_t EXT_CLOCK = 0;
    constexpr uint32_t FPGA_CLOCK = 1;
    constexpr uint32_t TCXO_CLOCK = 2;
    constexpr uint32_t PIN_SELECT = 3;
    constexpr uint32_t AUTO_CLOCK = 4;

    constexpr auto clkin_names = std::array{
        "External"sv,
        "FPGA"sv,
        "TCXO"sv,
        "Pin Select"sv,
        "Auto"sv
    };

    // Sampling frequency 200 MHz (f_vco = 2400 MHz)
    constexpr auto fs_200MHz = std::to_array<uint32_t>({
        2,   // PLL2_P
        12,  // PLL2_N
        2,   // PLL2_R
        240, // CLKout0_DIV (CLKOUT)
        12,  // CLKout1_DIV (ADC1 clock)
        12,  // CLKout2_DIV (ADC0 clock)
        12,  // CLKout3_DIV (FPGA clock)
        240, // CLKout4_DIV (EXP_CLK0 clock)
        240, // CLKout5_DIV (EXP_CLK1 clock)
        210  // MMCM phase shift
    });

    // Sampling frequency 250 MHz (f_vco = 2500 MHz)
    constexpr auto fs_250MHz = std::to_array<uint32_t>({
        5,   // PLL2_P
        5,   // PLL2_N
        2,   // PLL2_R
        250, // CLKout0_DIV (CLKOUT)
        10,  // CLKout1_DIV (ADC1 clock)
        10,  // CLKout2_DIV (ADC0 clock)
        10,  // CLKout3_DIV (FPGA clock)
        250, // CLKout4_DIV (EXP_CLK0 clock)
        250, // CLKout5_DIV (EXP_CLK1 clock)
        50   // MMCM phase shift
    });

    // Sampling frequency 240 MHz (f_vco = 2400 MHz)
    constexpr auto fs_240MHz = std::to_array<uint32_t>({
        2,   // PLL2_P
        12,  // PLL2_N
        2,   // PLL2_R
        240, // CLKout0_DIV (CLKOUT)
        10,  // CLKout1_DIV (ADC1 clock)
        10,  // CLKout2_DIV (ADC0 clock)
        10,  // CLKout3_DIV (FPGA clock)
        240, // CLKout4_DIV (EXP_CLK0 clock)
        240, // CLKout5_DIV (EXP_CLK1 clock)
        40    // MMCM phase shift
    });

    // Sampling frequency 100 MHz (f_vco = 2400 MHz)
    constexpr auto fs_100MHz = std::to_array<uint32_t>({
        2,   // PLL2_P
        12,  // PLL2_N
        2,   // PLL2_R
        240, // CLKout0_DIV (CLKOUT)
        24,  // CLKout1_DIV (ADC clock)
        24,  // CLKout2_DIV (DAC clock)
        24,  // CLKout3_DIV (FPGA clock)
        240, // CLKout4_DIV (EXP_CLK0 clock)
        240, // CLKout5_DIV (EXP_CLK1 clock)
        125  // MMCM phase shift
    });

    constexpr auto configs = std::array{fs_200MHz, fs_250MHz, fs_240MHz, fs_100MHz};
    constexpr size_t num_params = configs[0].size();
}

class Eeprom;
class SpiConfig;

class ClockGenerator
{
  public:
    ClockGenerator();

    void phase_shift(uint32_t n_shifts);
    int32_t set_tcxo_calibration(uint8_t new_cal);
    int32_t set_tcxo_clock(uint8_t value);
    void init();

    // 0: Ext. clock, 1: FPGA clock, 2: TCXO, 4: Automatic
    void set_reference_clock(uint32_t clkin_);
    void set_sampling_frequency(uint32_t fs_select);

    auto get_adc_sampling_freq() const {
        return fs_adc;
    }

    uint32_t get_reference_clock() const {
        return clkin;
    }

  private:
    hw::Memory<mem::control>& ctl;
    Eeprom& eeprom;
    SpiConfig& spi_cfg;

    const char* filename = "/tmp/clock-generator-initialized";
    bool is_clock_generator_initialized = true;

    uint32_t clkin = clock_cfg::TCXO_CLOCK; // Current input clock
    uint32_t fs_selected = clock_cfg::configs.size(); // Current frequency configuration
    std::array<uint32_t, clock_cfg::num_params> clk_cfg;

    static constexpr double f_vcxo = 1E8;   // VCXO frequency (Hz)
    static constexpr double fs_max = 2.5E8; // Max. sampling frequency to avoid overclocking
    double f_vco;  // VCO frequency (Hz)
    std::array<double, 2> fs_adc; // ADC sampling frequencies (Hz)

    // AD5141 non volatile digital potentiometer for TCXO frequency adjustment
    static constexpr uint32_t i2c_address = 0b0101111;

    void single_phase_shift(uint32_t incdec);
    void write_reg(uint32_t data);

    enum CLK_CONFIG_MODE {
        CLKIN_SELECT,
        SAMPLING_FREQ_SET,
        CFG_ALL
    };

    int configure(uint32_t cfg_mode, uint32_t clkin_select, const std::array<uint32_t, clock_cfg::num_params>& clk_cfg_);
};

#endif // __ALPHA250_4_DRIVERS_CLOCK_GENERATOR_HPP__
