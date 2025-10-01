#ifndef __ALPHA_DRIVERS_CLOCK_GENERATOR_HPP__
#define __ALPHA_DRIVERS_CLOCK_GENERATOR_HPP__

#include <context.hpp>

#include "./eeprom.hpp"
#include "./spi-config.hpp"

#include <array>
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
        12,  // CLKout1_DIV (ADC clock)
        12,  // CLKout2_DIV (DAC clock)
        12,  // CLKout3_DIV (FPGA clock)
        240, // CLKout4_DIV (EXP_CLK0 clock)
        240, // CLKout5_DIV (EXP_CLK1 clock)
        0    // MMCM phase shift
    });

    // Sampling frequency 250 MHz (f_vco = 2500 MHz)
    constexpr auto fs_250MHz = std::to_array<uint32_t>({
        5,   // PLL2_P
        5,   // PLL2_N
        2,   // PLL2_R
        250, // CLKout0_DIV (CLKOUT)
        10,  // CLKout1_DIV (ADC clock)
        10,  // CLKout2_DIV (DAC clock)
        10,  // CLKout3_DIV (FPGA clock)
        250, // CLKout4_DIV (EXP_CLK0 clock)
        250, // CLKout5_DIV (EXP_CLK1 clock)
        56   // MMCM phase shift
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
        120  // MMCM phase shift
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
        40   // MMCM phase shift
    });
    
    constexpr auto configs = std::array{fs_200MHz, fs_250MHz, fs_100MHz, fs_240MHz};
    constexpr size_t num_params = configs[0].size();
}

class ClockGenerator
{
  public:
    ClockGenerator(Context& ctx_);
    void phase_shift(uint32_t n_shifts);
    int32_t set_tcxo_calibration(uint8_t new_cal);
    int32_t set_tcxo_clock(uint8_t value);
    void init();

    // 0: Ext. clock, 1: FPGA clock, 2: TCXO, 4: Automatic
    void set_reference_clock(uint32_t clkin_);

    void set_sampling_frequency(uint32_t fs_select);
    double get_adc_sampling_freq() const;
    double get_dac_sampling_freq() const;
    uint32_t get_reference_clock() const;

  private:
    Context& ctx;
    Memory<mem::control>& ctl;
    I2cDev& i2c;
    Eeprom& eeprom;
    SpiConfig& spi_cfg;

    static constexpr auto filename = "/tmp/clock-generator-initialized"sv;
    bool is_clock_generator_initialized = true;

    uint32_t clkin = clock_cfg::TCXO_CLOCK; // Current input clock
    uint32_t fs_selected = clock_cfg::configs.size(); // Current frequency configuration
    std::array<uint32_t, clock_cfg::num_params> clk_cfg;

    static constexpr double f_vcxo = 1E8;   // VCXO frequency (Hz)
    static constexpr double fs_max = 2.5E8; // Max. sampling frequency to avoid overclocking
    double f_vco;  // VCO frequency (Hz)
    double fs_adc; // ADC sampling frequency (Hz)
    double fs_dac; // DAC sampling frequency (Hz)

    // AD5141 non volatile digital potentiometer for TCXO frequency adjustment
    static constexpr uint32_t i2c_address = 0b0101111;

    enum CLK_CONFIG_MODE {
        CLKIN_SELECT,
        SAMPLING_FREQ_SET,
        CFG_ALL
    };

    void single_phase_shift(uint32_t incdec);
    void write_reg(uint32_t data);
    int configure(uint32_t cfg_mode, uint32_t clkin_select, const std::array<uint32_t, clock_cfg::num_params>& clk_cfg_);
};

#endif // __ALPHA_DRIVERS_CLOCK_GENERATOR_HPP__
