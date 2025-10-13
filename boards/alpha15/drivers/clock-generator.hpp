#ifndef __ALPHA15_DRIVERS_CLOCK_GENERATOR_HPP__
#define __ALPHA15_DRIVERS_CLOCK_GENERATOR_HPP__

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

    enum ClkCfg {
        PLL2_P,
        PLL2_N,
        PLL2_R,
        CLKout0_DIV,
        CLKout1_DIV,
        CLKout2_DIV,
        CLKout3_DIV,
        CLKout4_DIV,
        CLKout5_DIV,
        MMCM_PHASE_SHIFT,
        ClkCfgNum
    };

    constexpr size_t num_params = ClkCfgNum;

    // Sampling frequency 15 MHz (f_vco = 2400 MHz)
    constexpr auto fs_15MHz = std::to_array<uint32_t>({
        2,   // PLL2_P
        12,  // PLL2_N
        2,   // PLL2_R
        240, // CLKout0_DIV (CLKOUT 10 MHz)
        160, // CLKout1_DIV (ADC0 clock 15 MHz)
        10,  // CLKout2_DIV (DAC clock 240 MHz)
        10,  // CLKout3_DIV (FPGA clock 240 MHz)
        160, // CLKout4_DIV (ADC1 clock 15 MHz)
        240, // CLKout5_DIV (EXP_CLK clock 10 MHz)
        0    // MMCM phase shift
    });

    constexpr std::array<std::array<uint32_t, num_params>, 1> configs = {fs_15MHz};
}

class I2cDev;
class Eeprom;
class SpiConfig;

class ClockGenerator
{
  public:
    ClockGenerator();

    void phase_shift(int32_t n_shifts);
    int32_t set_tcxo_calibration(uint8_t new_cal);
    int32_t set_tcxo_clock(uint8_t value);
    void init();
    void set_reference_clock(uint32_t clkin_); // 0: Ext. clock, 1: FPGA clock, 2: TCXO, 4: Automatic
    void set_sampling_frequency(uint32_t fs_select);

    auto get_adc_sampling_freq() {
        return fs_adc;
    }

    double get_dac_sampling_freq() {
        return fs_dac;
    }

    uint32_t get_reference_clock() {
        return clkin;
    }

    int32_t get_total_phase_shift() {
        return total_phase_shift;
    }

  private:
    I2cDev& i2c;
    Eeprom& eeprom;
    SpiConfig& spi_cfg;

    int32_t total_phase_shift = 0;

    static constexpr auto filename = "/tmp/clock-generator-initialized"sv;
    bool is_clock_generator_initialized = true;

    uint32_t clkin = clock_cfg::TCXO_CLOCK; // Current input clock
    uint32_t fs_selected = clock_cfg::configs.size(); // Current frequency configuration
    std::array<uint32_t, clock_cfg::num_params> clk_cfg;

    static constexpr double f_vcxo = 1E8;       // VCXO frequency (Hz)
    static constexpr double fs_dac_max = 2.5E8; // Max. sampling frequency to avoid DAC overclocking
    static constexpr double fs_adc_max = 15E6;  // Max. sampling frequency to avoid ADC overclocking
    double f_vco;                               // VCO frequency (Hz)
    std::array<double, 2> fs_adc;               // ADC sampling frequency (Hz)
    double fs_dac;                              // DAC sampling frequency (Hz)

    enum CLK_CONFIG_MODE {
        CLKIN_SELECT,
        SAMPLING_FREQ_SET,
        CFG_ALL
    };

    bool phase_shift_done();
    void write_reg(uint32_t data);
    int configure(uint32_t cfg_mode, uint32_t clkin_select, const std::array<uint32_t, clock_cfg::num_params>& clk_cfg_);
};

#endif // __ALPHA15_DRIVERS_CLOCK_GENERATOR_HPP__