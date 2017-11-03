/// Ad9747 driver
///
/// (c) Koheron

#ifndef __DRIVERS_AD9747_HPP__
#define __DRIVERS_AD9747_HPP__

#include <context.hpp>

#include <array>

// http://cds.linear.com/docs/en/datasheet/21576514fb.pdf

class Ad9747
{
  public:
    Ad9747(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    {}

    enum regs {
        SPI_CONTROL     = 0x00,
        DATA_CONTROL    = 0x02,
        POWER_DOWN      = 0x03,
        DAC_MODE_SELECT = 0x0A,
        DAC1_GAIN_LSB   = 0x0B,
        DAC1_GAIN_MSB   = 0x0C,
        AUX_DAC1_LSB    = 0x0D,
        AUX_DAC1_MSB    = 0x0E,
        DAC2_GAIN_LSB   = 0x0F,
        DAC2_GAIN_MSB   = 0x10,
        AUX_DAC2_LSB    = 0x11,
        AUX_DAC2_MSB    = 0x12
    };

    void init() {
        reset();
        write_reg((POWER_DOWN << 8) + 0b10110000);
        write_reg((DATA_CONTROL << 8) + (0 << 7) + (0 << 3));
        write_reg((DAC_MODE_SELECT << 8) + 0x00); // Normal mode
        set_dac_gain(0x01F9);
        //set_dac_gain(0x0040);
        //set_dac_gain(0x0100); 1Vpp
        //set_dac_gain(0x03FF);
    }

    void reset() {
        write_reg((SPI_CONTROL << 8) + 0b00100000);
        write_reg((SPI_CONTROL << 8) + 0x0);
    }

    void set_dac_gain(uint32_t gain) {
        write_reg((DAC1_GAIN_LSB << 8) + (gain & 0xFF));
        write_reg((DAC1_GAIN_MSB << 8) + (gain >> 8));
        write_reg((DAC2_GAIN_LSB << 8) + (gain & 0xFF));
        write_reg((DAC2_GAIN_MSB << 8) + (gain >> 8));
    }

    void write_reg(uint32_t data) {
      do {} while (sts.read<reg::spi_cfg_sts>() == 0);
      uint32_t cmd = (1 << TVALID_IDX) + (2 << 2) + 1;
      ctl.write<reg::spi_cfg_data>(data << 16);
      ctl.write<reg::spi_cfg_cmd>(cmd);
      ctl.clear_bit<reg::spi_cfg_cmd, TVALID_IDX>();
    }

  private:
    static constexpr uint32_t TVALID_IDX = 8;

    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
};

#endif // __DRIVERS_LTC2157_HPP__
