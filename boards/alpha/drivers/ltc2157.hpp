/// Ltc2157 driver
///
/// (c) Koheron

#ifndef __DRIVERS_LTC2157_HPP__
#define __DRIVERS_LTC2157_HPP__

#include <context.hpp>

// http://cds.linear.com/docs/en/datasheet/21576514fb.pdf

class Ltc2157
{
  public:
    Ltc2157(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    {}

    enum regs {
        RESET,
        POWER_DOWN,
        TIMING,
        OUTPUT_MODE,
        DATA_FORMAT
    };

    void init() {
        // Reset
        write_reg((RESET << 8) + (1 << 7));

        // Power down
        uint32_t SLEEP = 0;
        uint32_t NAP = 0;
        uint32_t PDB = 0;
        write_reg((POWER_DOWN << 8) + (SLEEP << 3) + (NAP << 2) + (PDB << 1));

        // Output mode
        uint32_t ILVDS = 0b111;
        uint32_t TERMON = 1;
        uint32_t OUTOFF = 0;
        write_reg((OUTPUT_MODE << 8) + (ILVDS << 2) + (TERMON << 1) + (OUTOFF << 0));

        // Timing
        uint32_t DELAY = 0;
        uint32_t DCS = 0;
        write_reg((TIMING << 8) + (DELAY << 1) + (DCS << 0));

        // Data format
        uint32_t RAND = 1;
        write_reg((DATA_FORMAT << 8) + (RAND << 1));
    }

    void set_timing(uint32_t delay) {
        uint32_t DCS = 0;
        write_reg((TIMING << 8) + (delay << 1) + (DCS << 0));
    }

    void write_reg(uint32_t data) {
      do {} while (sts.read<reg::spi_cfg_sts>() == 0);
      uint32_t cmd = (1 << TVALID_IDX) + (2 << 2) + 2;
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
