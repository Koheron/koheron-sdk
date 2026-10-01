/// XADC driver
/// (c) Koheron

#ifndef __DRIVERS_XADC_HPP__
#define __DRIVERS_XADC_HPP__

#include "server/hardware/memory_manager.hpp"

#include <algorithm>
#include <iterator>

/// http://www.xilinx.com/support/documentation/user_guides/ug480_7Series_XADC.pdf
namespace Xadc_regs {
    constexpr uint32_t set_chan     = 0x324;
    constexpr uint32_t avg_en       = 0x32C;
    constexpr uint32_t read         = 0x240;
    constexpr uint32_t config0      = 0x300;
    constexpr uint32_t TEMP         = 0x200;    // fpga temperature
    constexpr uint32_t PLVCCINT     = 0x204;    // fpga PL Vccint
    constexpr uint32_t PLVCCAUX     = 0x208;    // fpga PL Vccaux
    constexpr uint32_t PLVCCBRAM    = 0x218;    // fpga PL Vccbram
    constexpr uint32_t PSVCCINT     = 0x234;    // fpga PS Vccint
    constexpr uint32_t PSVCCAUX     = 0x238;    // fpga PS Vccaux
    constexpr uint32_t PSVCCMEM     = 0x23c;    // fpga PS Vccmem
}

class Xadc
{
  public:
    Xadc()
    : xadc(hw::get_memory<mem::xadc>())
    {
      enable_averaging();
      set_averaging(256);
    }

    void enable_averaging() {
        xadc.write<Xadc_regs::avg_en>((1 << channel_0) + (1 << channel_1));
    }

    void set_averaging(uint32_t n_avg) {
        uint32_t avg;
        constexpr uint32_t mask = (1 << 12) + (1 << 13);
        switch (n_avg) {
          case 1:
            avg = 0;
            break;
          case 4:
            avg = 1;
            break;
          case 64:
            avg = 2;
            break;
          case 256:
            avg = 3;
            break;
          default:
            avg = 0;
        }
        xadc.write_mask<Xadc_regs::config0, mask>(avg << 12);
    }

    uint32_t read(uint32_t channel) {
        if (channel != channel_0 && channel != channel_1) {
            return 0;
        }
        return xadc.read_reg(Xadc_regs::read + 4 * channel);
    }

  float get_temperature() {
    float ret = (xadc.read<Xadc_regs::TEMP>() * 503.975) / 65356 - 273.15;
    return ret;
  }
  // return FPGA PL Vccint voltage
  float get_PlVccInt() {
    return (xadc.read<Xadc_regs::PLVCCINT>() * 3.0) / 65356;
  }

  // return FPGA PL Vccaux voltage
  float get_PlVccAux() {
    return (xadc.read<Xadc_regs::PLVCCAUX>() * 3.0) / 65356;
  }

  // return FPGA PL Vbram voltage
  float get_PlVccBram() {
    return (xadc.read<Xadc_regs::PLVCCBRAM>() * 3.0) / 65356;
  }

  // return FPGA PS Vccint voltage
  float get_PsVccInt() {
    return (xadc.read<Xadc_regs::PSVCCINT>() * 3.0) / 65356;
  }

  // return FPGA PS Vccaux voltage
  float get_PsVccAux() {
    return (xadc.read<Xadc_regs::PSVCCAUX>() * 3.0) / 65356;
  }

  // return FPGA PS Vccmem voltage
  float get_PsVccMem() {
    return (xadc.read<Xadc_regs::PSVCCMEM>() * 3.0) / 65356;
  }

 private:
  hw::Memory<mem::xadc>& xadc;

  const uint32_t channel_0 = 1;
  const uint32_t channel_1 = 8;
};

#endif //__DRIVERS_XADC_HPP__
