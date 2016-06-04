/// XADC driver
///
/// http://www.xilinx.com/support/documentation/ip_documentation/xadc_wiz/v3_0/pg091-xadc-wiz.pdf
/// http://www.xilinx.com/support/documentation/user_guides/ug480_7Series_XADC.pdf
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_XADC_HPP__
#define __DRIVERS_CORE_XADC_HPP__

#include <drivers/lib/dev_mem.hpp>

#define XADC_ADDR          0x43C00000
#define XADC_RANGE         65536

// Offsets
// Set by Xilinx IP
#define SET_CHAN_OFF       0x324
#define AVG_EN_OFF         0x32C
#define READ_OFF           0x240
#define AVG_OFF            0x300

class Xadc
{
  public:
    Xadc(Klib::DevMem& dvm_);

    int Open() {return status == FAILED ? -1: 0;}
    int set_channel(uint32_t channel_0_, uint32_t channel_1_);
    
    void enable_averaging() {
        dvm.write32(xadc_map, AVG_EN_OFF, (1 << channel_0) + (1 << channel_1));
    }

    int set_averaging(uint32_t n_avg);
    int read(uint32_t channel);

    enum Status {
        CLOSED,
        OPENED,
        FAILED
    };

    #pragma tcp-server is_failed
    bool IsFailed() const {return status == FAILED;}

  private:
    Klib::DevMem& dvm;
    int status;

    // Memory maps IDs
    Klib::MemMapID xadc_map;

    uint32_t channel_0 = 1;
    uint32_t channel_1 = 8;
}; // class Xadc

#endif //__DRIVERS_CORE_XADC_HPP__
