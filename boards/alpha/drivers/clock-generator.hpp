#ifndef __CLOCK_GENERATOR_HPP__
#define __CLOCK_GENERATOR_HPP__

#include <context.hpp>

class ClockGenerator
{
  public:
    ClockGenerator(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , i2c(ctx.i2c.get("i2c-0"))
    {}

    void write_reg(uint32_t data) {
      do {} while (sts.read<reg::spi_cfg_sts>() == 0);
      uint32_t cmd = (1 << TVALID_IDX) + (0 << 2) + 0;
      ctl.write<reg::spi_cfg_data>(data);
      ctl.write<reg::spi_cfg_cmd>(cmd);
      ctl.clear_bit<reg::spi_cfg_cmd, TVALID_IDX>();
    }

    int32_t adjust_clock(uint8_t value) {
        std::array<uint8_t, 2> buff {0b00010000, value};
        return i2c.write(i2c_address, buff);
    }

    void init() {
        adjust_clock(95);

        // Start programming sequence
        write_reg(1 << 17); // Reset

        // R0: CLKOUT
        uint32_t CLKout0_DIV = 250; // Clock divisor 2.5 GHz / 250 = 10 MHz
        uint32_t CLKout0_DDLY = 0;  // Digital delay
        uint32_t CLKout0_PD = 0;    // Power down
        write_reg((CLKout0_PD << 31) + (CLKout0_DDLY << 18) + (CLKout0_DIV << 5) + 0);

        // R1: ADC clock
        uint32_t CLKout1_DIV = 10; // Clock divisor 2.5 GHz / 10 = 250 MHz
        uint32_t CLKout1_DDLY = 0; // Digital delay
        uint32_t CLKout1_PD = 0;   // Power down
        write_reg((CLKout1_PD << 31) + (CLKout1_DDLY << 18) + (CLKout1_DIV << 5) + 1);

        // R2: DAC clock
        uint32_t CLKout2_DIV = 10;  // Clock divisor 2.5 GHz / 10 = 250 MHz
        uint32_t CLKout2_DDLY = 8;  // Digital delay
        uint32_t CLKout2_PD = 0;    // Power down
        write_reg((CLKout2_PD << 31) + (CLKout2_DDLY << 18) + (CLKout2_DIV << 5) + 2);

        // R3: FPGA clock
        uint32_t CLKout3_DIV = 10;  // Clock divisor 2.5 GHz / 10 = 250 MHz
        uint32_t CLKout3_DDLY = 0;  // Digital delay
        uint32_t CLKout3_PD = 0;    // Power down
        write_reg((CLKout3_PD << 31) + (CLKout3_DDLY << 18) + (CLKout3_DIV << 5) + 3);

        // R4: EXP_CLK0 clock
        uint32_t CLKout4_DIV = 250;  // Clock divisor 2.5 GHz / 250 = 10 MHz
        uint32_t CLKout4_DDLY = 0;  // Digital delay
        uint32_t CLKout4_PD = 1;    // Power down
        write_reg((CLKout4_PD << 31) + (CLKout4_DDLY << 18) + (CLKout4_DIV << 5) + 4);

        // R5: EXP_CLK1 clock
        uint32_t CLKout5_DIV = 250;  // Clock divisor 2.5 GHz / 250 = 10 MHz
        uint32_t CLKout5_DDLY = 0;  // Digital delay
        uint32_t CLKout5_PD = 1;    // Power down
        write_reg((CLKout5_PD << 31) + (CLKout5_DDLY << 18) + (CLKout5_DIV << 5) + 5);

        // R6
        uint32_t CLKout0_TYPE = 8; // LVCMOS Norm/Norm
        uint32_t CLKout0_ADLY = 0; // Analog delay
        uint32_t CLKout1_TYPE = 1; // LVDS
        uint32_t CLKout1_ADLY = 0; // Analog delay
        write_reg((CLKout1_TYPE << 24) + (CLKout0_TYPE << 20) + (CLKout1_ADLY << 11) + (CLKout0_ADLY << 5) + 6);

        // R7
        uint32_t CLKout2_TYPE = 1; // LVDS
        uint32_t CLKout2_ADLY = 0; // Analog delay
        uint32_t CLKout3_TYPE = 1; // LVDS
        uint32_t CLKout3_ADLY = 0; // Analog delay
        write_reg((CLKout3_TYPE << 24) + (CLKout2_TYPE << 20) + (CLKout3_ADLY << 11) + (CLKout2_ADLY << 5) + 7);

        // R8
        uint32_t CLKout4_TYPE = 1; // LVDS
        uint32_t CLKout4_ADLY = 0; // Analog delay
        uint32_t CLKout5_TYPE = 1; // LVDS
        uint32_t CLKout5_ADLY = 0; // Analog delay
        write_reg((CLKout5_TYPE << 24) + (CLKout4_TYPE << 16) + (CLKout5_ADLY << 11) + (CLKout4_ADLY << 5) + 8);

        // R10
        uint32_t OSCout0_Type = 8; // LVCMOS Norm/Norm
        uint32_t EN_OSCout0 = 0;
        uint32_t OSCout0_MUX = 0; // 0: bypass OSC Divider
        uint32_t PD_OSCin = 0;
        uint32_t OSCout_DIV = 1; // 1: divide by 2
        uint32_t VCO_MUX = 0; // 0: bypass the VCO divider
        uint32_t EN_FEEDBACK_MUX = 0;
        uint32_t VCO_DIV = 2;
        uint32_t FEEDBACK_MUX = 0;
        write_reg((1 << 28) + (OSCout0_Type << 24) + (EN_OSCout0 << 22) +  (OSCout0_MUX << 20)
                  + (PD_OSCin << 19) + (OSCout_DIV << 16) + (1 << 14) + (VCO_MUX << 12)
                  + (EN_FEEDBACK_MUX << 0) + (VCO_DIV << 8) + (FEEDBACK_MUX << 5) + 10);

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
        write_reg((Mode << 27) + (EN_SYNC << 26) + (NO_SYNC_CLKout5 << 25) + (NO_SYNC_CLKout4 << 24) + (NO_SYNC_CLKout3 << 23)
                  + (NO_SYNC_CLKout2 << 22) + (NO_SYNC_CLKout1 << 21) + (NO_SYNC_CLKout0 << 20) + (SYNC_CLKin2_MUX << 18)
                  + (SYNC_QUAL << 17) + (SYNC_POL_INV << 16) + (SYNC_EN_AUTO << 15) + (SYNC_TYPE << 12) + (EN_PLL2_XTAL << 5) + 11);

        // R12
        uint32_t LD_MUX = 3;
        uint32_t LD_TYPE = 3;
        uint32_t SYNC_PLL2_DLD = 0;
        uint32_t SYNC_PLL1_DLD = 0;
        uint32_t EN_TRACK = 1;
        uint32_t HOLDOVER_MODE = 2;
        write_reg((LD_MUX << 27) + (LD_TYPE << 24) + (SYNC_PLL2_DLD << 23) + (SYNC_PLL1_DLD << 22)
                  + (1 << 19) + (1 << 18) + (EN_TRACK << 8) + (HOLDOVER_MODE << 6) + 12);

        // R13
        uint32_t HOLDOVER_MUX = 7;
        uint32_t HOLDOVER_TYPE = 3;
        uint32_t Status_CLKin1_MUX = 0;
        uint32_t Status_CLKin0_TYPE = 2;
        uint32_t DISABLE_DLD1_DET = 0;
        uint32_t Status_CLKin0_MUX = 0;
        uint32_t CLKin_SELECT_MODE = 4; // Automatic
        uint32_t CLKin_Sel_INV =0;
        uint32_t EN_CLKin2 = 1; // CLKin2 usable
        uint32_t EN_CLKin1 = 1; // CLKin1 usable
        uint32_t EN_CLKin0 = 1; // CLKin0 usable
        write_reg((HOLDOVER_MUX << 27) + (HOLDOVER_TYPE << 24) + (Status_CLKin1_MUX << 20) + (Status_CLKin0_TYPE << 16)
                  + (DISABLE_DLD1_DET << 15) + (Status_CLKin0_MUX << 12) + (CLKin_SELECT_MODE << 9 ) + (CLKin_Sel_INV << 8)
                  + (EN_CLKin2 << 7) + (EN_CLKin1 << 6) + (EN_CLKin0 << 5) + 13);

        // R14
        uint32_t LOS_TIMEOUT = 0;
        uint32_t EN_LOS = 1;
        uint32_t Status_CLKin1_TYPE = 2;
        uint32_t CLKin2_BUF_TYPE = 0; // Bipolar
        uint32_t CLKin1_BUF_TYPE = 0; // Bipolar
        uint32_t CLKin0_BUF_TYPE = 0; // Bipolar
        uint32_t DAC_HIGH_TRIP = 0;
        uint32_t DAC_LOW_TRIP = 0;
        uint32_t EN_VTUNE_RAIL_DET = 0;
        write_reg((LOS_TIMEOUT << 30) + (EN_LOS << 28) + (Status_CLKin1_TYPE << 24) + (CLKin2_BUF_TYPE << 22) + (CLKin1_BUF_TYPE << 21)
                   + (CLKin0_BUF_TYPE << 20) + (DAC_HIGH_TRIP << 14) + (DAC_LOW_TRIP << 6) + (EN_VTUNE_RAIL_DET << 5) + 14);

        // R15
        uint32_t MAN_DAC = 100;
        uint32_t EN_MAN_DAC = 0;
        uint32_t HOLDOVER_DLD_CNT = 512;
        uint32_t FORCE_HOLDOVER = 0;
        write_reg((MAN_DAC << 22) + (EN_MAN_DAC << 20) + (HOLDOVER_DLD_CNT << 6) + (FORCE_HOLDOVER << 5) + 15);

        // R16
        uint32_t XTAL_LVL = 0;
        write_reg((XTAL_LVL << 30) + (1 << 24) + (1 << 22) + (1 << 20) + (1 << 18) + (1 << 16) + (1 << 10) + 16);

        // R24
        uint32_t PLL2_C4_LF = 0; // 10 pF
        uint32_t PLL2_C3_LF = 0; // 10 pF
        uint32_t PLL2_R4_LF = 0; // 200 Ohm
        uint32_t PLL2_R3_LF = 0; // 200 Ohm
        uint32_t PLL1_N_DLY = 0;
        uint32_t PLL1_R_DLY = 0;
        uint32_t PLL1_WND_SIZE = 3;
        write_reg((PLL2_C4_LF << 28) + (PLL2_C3_LF << 24) + (PLL2_R4_LF << 20) + (PLL2_R3_LF << 16)
                  + (PLL1_N_DLY << 12) + (PLL1_R_DLY << 8) + (PLL1_WND_SIZE << 6) + 24);

        // R25
        uint32_t DAC_CLK_DIV = 4;
        uint32_t PLL1_DLD_CNT = 1024;
        write_reg((DAC_CLK_DIV << 22) + (PLL1_DLD_CNT << 6) + 25);

        // R26
        uint32_t PLL2_WND_SIZE = 0;
        uint32_t EN_PLL2_REF_2X = 1; // Doubles reference frequency of PLL2
        uint32_t PLL2_CP_POL = 0; // Negative
        uint32_t PLL2_CP_GAIN = 3; // CP2 gain 3.2 mA
        uint32_t PLL2_DLD_CNT = 8192;
        uint32_t PLL2_CP_TRI = 0; // PLL2 charge pump active
        write_reg((PLL2_WND_SIZE << 30) + (EN_PLL2_REF_2X << 29) + (PLL2_CP_POL << 28) + (PLL2_CP_GAIN << 26)
                  + (1 << 25) + (1 << 24) + (1 << 23) + (1 << 21) + (PLL2_DLD_CNT << 6) + (PLL2_CP_TRI << 5) + 26);

        // R27
        uint32_t PLL1_CP_POL = 1; // CP1 polarity positive
        uint32_t PLL1_CP_GAIN = 3; // CP1 gain 1.6 mA
        uint32_t CLKin2_PreR_DIV = 0; // Prediv by 1
        uint32_t CLKin1_PreR_DIV = 0; // Prediv by 1
        uint32_t CLKin0_PreR_DIV = 0; // Prediv by 1
        uint32_t PLL1_R = 100;
        uint32_t PLL1_CP_TRI = 0; // Active
        write_reg((PLL1_CP_POL << 28) + (PLL1_CP_GAIN << 26) + (CLKin2_PreR_DIV << 24) + (CLKin1_PreR_DIV << 22)
                  + (CLKin0_PreR_DIV << 20) + (PLL1_R << 6) + (PLL1_CP_TRI << 5) + 27);

        // R28
        uint32_t PLL2_R = 2; // Divide by 2
        uint32_t PLL1_N = 1000;
        write_reg( (PLL2_R << 20) + (PLL1_N << 6) + 28);

        // R29
        uint32_t OSCin_FREQ = 1; // 63 MHz < VCXO freq (100 MHz) < 127 MHz
        uint32_t PLL2_FAST_PDF = 0; // PDF = 100 MHz
        uint32_t PLL2_N_CAL = 5;
        write_reg((OSCin_FREQ << 24) + (PLL2_FAST_PDF << 23) + (PLL2_N_CAL << 5) + 29);

        // R30
        uint32_t PLL2_P = 5;
        uint32_t PLL2_N = 5;
        write_reg((PLL2_P << 24) + (PLL2_N << 5) + 30);
    }

  private:
    static constexpr uint32_t TVALID_IDX = 8;

    // AD5141 non volatile digital potentiometer for TCXO frequency adjustment
    static constexpr uint32_t i2c_address = 0b010'1111;

    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    I2cDev& i2c;

};

#endif // __CLOCK_GENERATOR_HPP__