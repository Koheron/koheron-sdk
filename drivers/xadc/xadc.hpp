/// XADC driver
///
/// http://www.xilinx.com/support/documentation/ip_documentation/xadc_wiz/v3_0/pg091-xadc-wiz.pdf
/// http://www.xilinx.com/support/documentation/user_guides/ug480_7Series_XADC.pdf
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_XADC_HPP__
#define __DRIVERS_CORE_XADC_HPP__

#include <drivers/lib/dev_mem.hpp>

// Offsets
// Set by Xilinx IP
#define SET_CHAN_OFF       0x324
#define AVG_EN_OFF         0x32C
#define READ_OFF           0x240
#define XADC_CFG0_OFF      0x300
#define XADC_CFG1_OFF      0x304
#define XADC_CFG2_OFF      0x308

class Xadc
{
  public:
    Xadc(DevMem& dvm_);

    int set_channel(uint32_t channel_0_, uint32_t channel_1_);
    
    void enable_averaging() {
        xadc.write<AVG_EN_OFF>((1 << channel_0) + (1 << channel_1));
    }

    int set_averaging(uint32_t n_avg);
    int read(uint32_t channel);

  private:
    DevMem& dvm;
    MemoryMap<XADC_MEM>& xadc;

    uint32_t channel_0 = 1;
    uint32_t channel_1 = 8;
}; // class Xadc

#endif //__DRIVERS_CORE_XADC_HPP__
