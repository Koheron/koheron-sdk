/// XADC driver
///
/// http://www.xilinx.com/support/documentation/ip_documentation/xadc_wiz/v3_0/pg091-xadc-wiz.pdf
/// http://www.xilinx.com/support/documentation/user_guides/ug480_7Series_XADC.pdf
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_XADC_HPP__
#define __DRIVERS_CORE_XADC_HPP__

#include "dev_mem.hpp"
#include "wr_register.hpp"

#define XADC_ADDR          0x43C00000

// Offsets
// Set by Xilinx IP
#define SET_CHAN_OFF       0x324
#define AVG_EN_OFF         0x32C
#define READ_OFF           0x240
#define AVG_OFF            0x300

//> \description XADC driver
class Xadc
{
  public:
    Xadc(Klib::DevMem& dev_mem_);
    
    ~Xadc();

    //> \description Open the device
    //> \io_type WRITE
    //> \param map_size Size of the device memory to be mapped
    //> \status ERROR_IF_NEG
    //> \on_error Cannot open XADC device
    //> \flag AT_INIT
    int Open(uint32_t map_size = 16*4096);
    
    void Close();

    //> \description Select 2 channels among the four available channels (0, 1, 8 and 9)
    //> \io_type WRITE
    //> \param channel_0_ (0, 1, 8 or 9)
    //> \param channel_1_ (0, 1, 8 or 9)
    //> \status ERROR_IF_NEG
    //> \on_error Invalid XADC channel
    int set_channel(uint32_t channel_0_, uint32_t channel_1_);

    //> \description Enable averaging
    //> \io_type WRITE
    //> \status NEVER_FAIL
    void enable_averaging();

    //> \description Set number of points averages
    //> \io_type WRITE
    //> \param n_avg Number of averages (1, 4, 64 or 256)
    //> \status ERROR_IF_NEG
    //> \on_error Invalid XADC averaging value
    int set_averaging(uint32_t n_avg);

    //> \description Read XADC value
    //> \io_type READ
    //> \param channel Channel to be read
    //> \status ERROR_IF_NEG
    //> \on_error Invalid XADC channel
    int read(uint32_t channel);

    enum Status {
        CLOSED,
        OPENED,
        FAILED
    };

    //> \is_failed
    bool IsFailed() const {return status == FAILED;}

  private:
    Klib::DevMem& dev_mem;

    int status;

    // Memory maps IDs:
    Klib::MemMapID dev_num;

    uint32_t channel_0 = 1;
    uint32_t channel_1 = 8;
}; // class Xadc

#endif //__DRIVERS_CORE_XADC_HPP__
