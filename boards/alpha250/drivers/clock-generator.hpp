#ifndef __ALPHA_DRIVERS_CLOCK_GENERATOR_HPP__
#define __ALPHA_DRIVERS_CLOCK_GENERATOR_HPP__

#include <context.hpp>

#include "eeprom.hpp"
#include "spi-config.hpp"

#include <array>

namespace clock_cfg {
  constexpr size_t par_num = 9;

  // Sampling frequency 250 MHz (f_vco = 2500 MHz)
  constexpr std::array<uint32_t, par_num> fs_250MHz = {
        5,   // PLL2_P
        5,   // PLL2_N
        2,   // PLL2_R
        250, // CLKout0_DIV (CLKOUT)
        10,  // CLKout1_DIV (ADC clock)
        10,  // CLKout2_DIV (DAC clock)
        10,  // CLKout3_DIV (FPGA clock)
        250, // CLKout4_DIV (EXP_CLK0 clock)
        250  // CLKout5_DIV (EXP_CLK1 clock)
    };

  // Sampling frequency 200 MHz (f_vco = 2400 MHz)
  constexpr std::array<uint32_t, par_num> fs_200MHz = {
        2,   // PLL2_P
        12,  // PLL2_N
        2,   // PLL2_R
        240, // CLKout0_DIV (CLKOUT)
        12,  // CLKout1_DIV (ADC clock)
        12,  // CLKout2_DIV (DAC clock)
        12,  // CLKout3_DIV (FPGA clock)
        240, // CLKout4_DIV (EXP_CLK0 clock)
        240  // CLKout5_DIV (EXP_CLK1 clock)
    };
}

class ClockGenerator
{
  public:
    ClockGenerator(Context& ctx_)
    : ctx(ctx_)
    , i2c(ctx.i2c.get("i2c-0"))
    , eeprom(ctx.get<Eeprom>())
    , spi_cfg(ctx.get<SpiConfig>())
    {}

    int32_t set_tcxo_calibration(uint8_t new_cal) {
        tcxo_calibration = new_cal;
        set_tcxo_clock(tcxo_calibration);

        std::array<uint8_t, 1> cal_array {tcxo_calibration};
        return eeprom.write<eeprom_map::clock_generator_calib::offset>(cal_array);
    }

    int32_t set_tcxo_clock(uint8_t value) {
        std::array<uint8_t, 2> buff {0b00010000, value};
        return i2c.write(i2c_address, buff);
    }

    void init() {
        std::array<uint8_t, 1> cal_array;
        eeprom.read<eeprom_map::clock_generator_calib::offset>(cal_array);
        tcxo_calibration = cal_array[0];
        set_tcxo_clock(tcxo_calibration);

        configure(2, clock_cfg::fs_250MHz);
    }

    // 0: Ext. clock, 1: FPGA clock, 2: TCXO, 4: Automatic
    void set_reference_clock(uint32_t clkin_) {
        if (clkin_ != clkin) {
            configure(clkin_, clk_cfg);
        }
    }

    void set_sampling_frequency(uint32_t fs_select) {
        if (fs_select == 0) {
            configure(clkin, clock_cfg::fs_200MHz);
        } else {
            configure(clkin, clock_cfg::fs_250MHz);
        }
    }

    void configure(uint32_t clkin_select, const std::array<uint32_t, clock_cfg::par_num>& clk_cfg_) {
        if (clkin_select > 4) {
            ctx.log<ERROR>("Clock generator - Invalid reference clock source");
            return;
        }

        clkin = clkin_select;
        clk_cfg = clk_cfg_;

        // R0: CLKOUT
        uint32_t CLKout0_DIV = clk_cfg[3]; // Clock divisor 2.5 GHz / 250 = 10 MHz
        uint32_t CLKout0_DDLY = 0;  // Digital delay
        uint32_t CLKout0_PD = 0;    // Power down

        // R1: ADC clock
        uint32_t CLKout1_DIV = clk_cfg[4]; // Clock divisor 2.5 GHz / 10 = 250 MHz
        uint32_t CLKout1_DDLY = 0; // Digital delay
        uint32_t CLKout1_PD = 0;   // Power down

        // R2: DAC clock
        uint32_t CLKout2_DIV = clk_cfg[5];  // Clock divisor 2.5 GHz / 10 = 250 MHz
        uint32_t CLKout2_DDLY = 8;  // Digital delay
        uint32_t CLKout2_PD = 0;    // Power down

        // R3: FPGA clock
        uint32_t CLKout3_DIV = clk_cfg[6];  // Clock divisor 2.5 GHz / 10 = 250 MHz
        uint32_t CLKout3_DDLY = 0;  // Digital delay
        uint32_t CLKout3_PD = 0;    // Power down

        // R4: EXP_CLK0 clock
        uint32_t CLKout4_DIV = clk_cfg[7];  // Clock divisor 2.5 GHz / 250 = 10 MHz
        uint32_t CLKout4_DDLY = 0;  // Digital delay
        uint32_t CLKout4_PD = 1;    // Power down

        // R5: EXP_CLK1 clock
        uint32_t CLKout5_DIV = clk_cfg[8];  // Clock divisor 2.5 GHz / 250 = 10 MHz
        uint32_t CLKout5_DDLY = 0;  // Digital delay
        uint32_t CLKout5_PD = 1;    // Power down

        // R6
        uint32_t CLKout0_TYPE = 8; // LVCMOS Norm/Norm
        uint32_t CLKout0_ADLY = 0; // Analog delay
        uint32_t CLKout1_TYPE = 1; // LVDS
        uint32_t CLKout1_ADLY = 0; // Analog delay

        // R7
        uint32_t CLKout2_TYPE = 1; // LVDS
        uint32_t CLKout2_ADLY = 0; // Analog delay
        uint32_t CLKout3_TYPE = 1; // LVDS
        uint32_t CLKout3_ADLY = 0; // Analog delay

        // R8
        uint32_t CLKout4_TYPE = 1; // LVDS
        uint32_t CLKout4_ADLY = 0; // Analog delay
        uint32_t CLKout5_TYPE = 1; // LVDS
        uint32_t CLKout5_ADLY = 0; // Analog delay

        // R10
        uint32_t OSCout0_Type = 8; // LVCMOS Norm/Norm
        uint32_t EN_OSCout0 = 0;
        uint32_t OSCout0_MUX = 0;  // 0: bypass OSC Divider
        uint32_t PD_OSCin = 0;
        uint32_t OSCout_DIV = 1;   // 1: divide by 2
        uint32_t VCO_MUX = 0;      // 0: bypass the VCO divider
        uint32_t EN_FEEDBACK_MUX = 0;
        uint32_t VCO_DIV = 2;
        uint32_t FEEDBACK_MUX = 0;

        // R11
        uint32_t Mode = 0; // Internal VCO
        // Sync needs to be ON for digital delay
        uint32_t EN_SYNC = 1;
        uint32_t NO_SYNC_CLKout5 = 0;
        uint32_t NO_SYNC_CLKout4 = 0;
        uint32_t NO_SYNC_CLKout3 = 0;
        uint32_t NO_SYNC_CLKout2 = 0;
        uint32_t NO_SYNC_CLKout1 = 0;
        uint32_t NO_SYNC_CLKout0 = 0;
        uint32_t SYNC_CLKin2_MUX = 0;
        uint32_t SYNC_QUAL = 0;
        uint32_t SYNC_POL_INV = 1;
        uint32_t SYNC_EN_AUTO = 0;
        uint32_t SYNC_TYPE = 1;
        uint32_t EN_PLL2_XTAL = 0; // We don't use a crystal for PLL2

        // R12
        uint32_t LD_MUX = 3;
        uint32_t LD_TYPE = 3;
        uint32_t SYNC_PLL2_DLD = 0;
        uint32_t SYNC_PLL1_DLD = 0;
        uint32_t EN_TRACK = 0;
        uint32_t HOLDOVER_MODE = 2;

        // R13
        uint32_t HOLDOVER_MUX = 7;
        uint32_t HOLDOVER_TYPE = 3;
        uint32_t Status_CLKin1_MUX = 0;
        uint32_t Status_CLKin0_TYPE = 2;
        uint32_t DISABLE_DLD1_DET = 0;
        uint32_t Status_CLKin0_MUX = 0;
        uint32_t CLKin_SELECT_MODE = clkin_select; // 0: Ext. clock, 1: FPGA clock, 2: TCXO, 4: Automatic
        uint32_t CLKin_Sel_INV =0;
        uint32_t EN_CLKin2 = 1; // CLKin2 usable
        uint32_t EN_CLKin1 = 1; // CLKin1 usable
        uint32_t EN_CLKin0 = 1; // CLKin0 usable

        // R14
        uint32_t LOS_TIMEOUT = 2;
        uint32_t EN_LOS = 1;
        uint32_t Status_CLKin1_TYPE = 2;
        uint32_t CLKin2_BUF_TYPE = 0; // TCXO is LVCMOS but use bipolar input since AC coupled
        uint32_t CLKin1_BUF_TYPE = 0; // Bipolar (Differential)
        uint32_t CLKin0_BUF_TYPE = 0; // Bipolar (Differential)
        uint32_t DAC_HIGH_TRIP = 0;
        uint32_t DAC_LOW_TRIP = 0;
        uint32_t EN_VTUNE_RAIL_DET = 0;

        // R15
        uint32_t MAN_DAC = 100;
        uint32_t EN_MAN_DAC = 0;
        uint32_t HOLDOVER_DLD_CNT = 512; //512
        uint32_t FORCE_HOLDOVER = 0;

        // R16
        uint32_t XTAL_LVL = 0;

        // R24
        uint32_t PLL2_C4_LF = 0; // 10 pF
        uint32_t PLL2_C3_LF = 0; // 10 pF
        uint32_t PLL2_R4_LF = 0; // 200 Ohm
        uint32_t PLL2_R3_LF = 0; // 200 Ohm
        uint32_t PLL1_N_DLY = 0;
        uint32_t PLL1_R_DLY = 0;
        uint32_t PLL1_WND_SIZE = 3;

        // R25
        uint32_t DAC_CLK_DIV = 4;
        uint32_t PLL1_DLD_CNT = 1024;

        // R26
        uint32_t PLL2_WND_SIZE = 2;
        uint32_t EN_PLL2_REF_2X = 1; // Doubles reference frequency of PLL2
        uint32_t PLL2_CP_POL = 0;    // Negative
        uint32_t PLL2_CP_GAIN = 3;   // CP2 gain 3.2 mA
        uint32_t PLL2_DLD_CNT = 8192;
        uint32_t PLL2_CP_TRI = 0;    // PLL2 charge pump active

        // R27
        uint32_t PLL1_CP_POL = 1;     // CP1 polarity positive
        uint32_t PLL1_CP_GAIN = 3;    // CP1 gain 1.6 mA
        uint32_t CLKin2_PreR_DIV = 0; // Prediv by 1
        uint32_t CLKin1_PreR_DIV = 0; // Prediv by 1
        uint32_t CLKin0_PreR_DIV = 0; // Prediv by 1
        uint32_t PLL1_R = 100;
        uint32_t PLL1_CP_TRI = 0;     // Active

        // R28
        uint32_t PLL2_R = clk_cfg[2];
        uint32_t PLL1_N = 1000;

        // R29
        uint32_t OSCin_FREQ = 1;    // 63 MHz < VCXO freq (100 MHz) < 127 MHz
        uint32_t PLL2_FAST_PDF = 0; // PDF = 100 MHz
        uint32_t PLL2_N_CAL = clk_cfg[1];

        // R30
        uint32_t PLL2_P = clk_cfg[0];
        uint32_t PLL2_N = clk_cfg[1];

        // Compute frequencies
        f_vco = f_vcxo * PLL2_P * PLL2_N / PLL2_R;

        if (EN_PLL2_REF_2X == 1) {
            f_vco *= 2;
        }

        if (f_vco < 2.37E9 || f_vco > 2.6E9) {
            ctx.log<ERROR>("Clock generator - VCO frequency at %f MHz is out of range (2370 to 2600 MHz)", f_vco * 1E-6);
            return;
        }

        fs_adc = f_vco / CLKout1_DIV;
        fs_dac = f_vco / CLKout2_DIV;

        if (fs_adc > fs_max || fs_dac > fs_max) {
            // We don't program the clock generator if it
            // results in data converters overclocking

            ctx.log<ERROR>("Clock generator - Data converters overclocking\n");
            return;
        }

        ctx.log<INFO>("Clock generator - VCO: %f MHz, ADC: %f MHz, DAC: %f MHz\n",
                      f_vco * 1E-6, fs_adc * 1E-6, fs_dac * 1E-6);

        // For each change the whole chip must be reprogram and the registers order must be respected
        write_reg(1 << 17); // Reset
        write_reg((CLKout0_PD << 31) + (CLKout0_DDLY << 18) + (CLKout0_DIV << 5) + 0);
        write_reg((CLKout1_PD << 31) + (CLKout1_DDLY << 18) + (CLKout1_DIV << 5) + 1);
        write_reg((CLKout2_PD << 31) + (CLKout2_DDLY << 18) + (CLKout2_DIV << 5) + 2);
        write_reg((CLKout3_PD << 31) + (CLKout3_DDLY << 18) + (CLKout3_DIV << 5) + 3);
        write_reg((CLKout4_PD << 31) + (CLKout4_DDLY << 18) + (CLKout4_DIV << 5) + 4);
        write_reg((CLKout5_PD << 31) + (CLKout5_DDLY << 18) + (CLKout5_DIV << 5) + 5);
        write_reg((CLKout1_TYPE << 24) + (CLKout0_TYPE << 20) + (CLKout1_ADLY << 11) + (CLKout0_ADLY << 5) + 6);
        write_reg((CLKout3_TYPE << 24) + (CLKout2_TYPE << 20) + (CLKout3_ADLY << 11) + (CLKout2_ADLY << 5) + 7);
        write_reg((CLKout5_TYPE << 24) + (CLKout4_TYPE << 16) + (CLKout5_ADLY << 11) + (CLKout4_ADLY << 5) + 8);
        write_reg((1 << 28) + (OSCout0_Type << 24) + (EN_OSCout0 << 22) +  (OSCout0_MUX << 20)
                  + (PD_OSCin << 19) + (OSCout_DIV << 16) + (1 << 14) + (VCO_MUX << 12)
                  + (EN_FEEDBACK_MUX << 0) + (VCO_DIV << 8) + (FEEDBACK_MUX << 5) + 10);
        write_reg((Mode << 27) + (EN_SYNC << 26) + (NO_SYNC_CLKout5 << 25) + (NO_SYNC_CLKout4 << 24) + (NO_SYNC_CLKout3 << 23)
                  + (NO_SYNC_CLKout2 << 22) + (NO_SYNC_CLKout1 << 21) + (NO_SYNC_CLKout0 << 20) + (SYNC_CLKin2_MUX << 18)
                  + (SYNC_QUAL << 17) + (SYNC_POL_INV << 16) + (SYNC_EN_AUTO << 15) + (SYNC_TYPE << 12) + (EN_PLL2_XTAL << 5) + 11);
        write_reg((LD_MUX << 27) + (LD_TYPE << 24) + (SYNC_PLL2_DLD << 23) + (SYNC_PLL1_DLD << 22)
                  + (1 << 19) + (1 << 18) + (EN_TRACK << 8) + (HOLDOVER_MODE << 6) + 12);
        write_reg((HOLDOVER_MUX << 27) + (HOLDOVER_TYPE << 24) + (Status_CLKin1_MUX << 20) + (Status_CLKin0_TYPE << 16)
                  + (DISABLE_DLD1_DET << 15) + (Status_CLKin0_MUX << 12) + (CLKin_SELECT_MODE << 9 ) + (CLKin_Sel_INV << 8)
                  + (EN_CLKin2 << 7) + (EN_CLKin1 << 6) + (EN_CLKin0 << 5) + 13);
        write_reg((LOS_TIMEOUT << 30) + (EN_LOS << 28) + (Status_CLKin1_TYPE << 24) + (CLKin2_BUF_TYPE << 22) + (CLKin1_BUF_TYPE << 21)
                   + (CLKin0_BUF_TYPE << 20) + (DAC_HIGH_TRIP << 14) + (DAC_LOW_TRIP << 6) + (EN_VTUNE_RAIL_DET << 5) + 14);
        write_reg((MAN_DAC << 22) + (EN_MAN_DAC << 20) + (HOLDOVER_DLD_CNT << 6) + (FORCE_HOLDOVER << 5) + 15);
        write_reg((XTAL_LVL << 30) + (1 << 24) + (1 << 22) + (1 << 20) + (1 << 18) + (1 << 16) + (1 << 10) + 16);
        write_reg((PLL2_C4_LF << 28) + (PLL2_C3_LF << 24) + (PLL2_R4_LF << 20) + (PLL2_R3_LF << 16)
                  + (PLL1_N_DLY << 12) + (PLL1_R_DLY << 8) + (PLL1_WND_SIZE << 6) + 24);
        write_reg((DAC_CLK_DIV << 22) + (PLL1_DLD_CNT << 6) + 25);
        write_reg((PLL2_WND_SIZE << 30) + (EN_PLL2_REF_2X << 29) + (PLL2_CP_POL << 28) + (PLL2_CP_GAIN << 26)
                  + (1 << 25) + (1 << 24) + (1 << 23) + (1 << 21) + (PLL2_DLD_CNT << 6) + (PLL2_CP_TRI << 5) + 26);
        write_reg((PLL1_CP_POL << 28) + (PLL1_CP_GAIN << 26) + (CLKin2_PreR_DIV << 24) + (CLKin1_PreR_DIV << 22)
                  + (CLKin0_PreR_DIV << 20) + (PLL1_R << 6) + (PLL1_CP_TRI << 5) + 27);
        write_reg( (PLL2_R << 20) + (PLL1_N << 6) + 28);
        write_reg((OSCin_FREQ << 24) + (PLL2_FAST_PDF << 23) + (PLL2_N_CAL << 5) + 29);
        write_reg((PLL2_P << 24) + (PLL2_N << 5) + 30);
    }

    double get_adc_sampling_freq() const {
        return fs_adc;
    }

    double get_dac_sampling_freq() const {
        return fs_dac;
    }

    uint32_t get_reference_clock() const {
        return clkin;
    }

  private:
    Context& ctx;
    I2cDev& i2c;
    Eeprom& eeprom;
    SpiConfig& spi_cfg;

    uint8_t tcxo_calibration;
    uint32_t clkin; // Current input clock
    std::array<uint32_t, clock_cfg::par_num> clk_cfg;

    static constexpr double f_vcxo = 1E8;   // VCXO frequency (Hz)
    static constexpr double fs_max = 2.5E8; // Max. sampling frequency to avoid overclocking
    double f_vco;  // VCO frequency (Hz)
    double fs_adc; // ADC sampling frequency (Hz)
    double fs_dac; // DAC sampling frequency (Hz)

    // AD5141 non volatile digital potentiometer for TCXO frequency adjustment
    static constexpr uint32_t i2c_address = 0b0101111;

    void write_reg(uint32_t data) {
        constexpr uint8_t cs_clk_gen = 0;
        constexpr uint8_t pack_size = 4; // bytes
        spi_cfg.write_reg<cs_clk_gen, pack_size>(data);
    }
};

#endif // __ALPHA_DRIVERS_CLOCK_GENERATOR_HPP__