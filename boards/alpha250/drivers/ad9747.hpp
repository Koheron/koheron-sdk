/// Ad9747 driver
///
/// (c) Koheron

#ifndef __ALPHA_DRIVERS_AD9747_HPP__
#define __ALPHA_DRIVERS_AD9747_HPP__

#include "server/runtime/services.hpp"
#include "server/runtime/drivers_manager.hpp"

#include "spi-config.hpp"

class Ad9747
{
  public:
    Ad9747()
    : spi_cfg(services::require<rt::DriverManager>().get<SpiConfig>())
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
        spi_cfg.lock();
        reset();
        write_reg((POWER_DOWN << 8) + 0b10110000);
        write_reg((DATA_CONTROL << 8) + (0 << 7) + (0 << 3));
        write_reg((DAC_MODE_SELECT << 8) + 0x00); // Normal mode
        set_dac_gain(0x1B8); // 1 Vpp
        spi_cfg.unlock();
    }

  private:
    SpiConfig& spi_cfg;

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
        spi_cfg.write_reg<1, 2>(data << 16);
    }
};

#endif // __ALPHA_DRIVERS_LTC2157_HPP__
