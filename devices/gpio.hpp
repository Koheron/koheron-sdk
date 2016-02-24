/// GPIO driver
///
/// http://www.xilinx.com/support/documentation/ip_documentation/axi_gpio/v2_0/pg144-axi-gpio.pdf
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_GPIO_HPP__
#define __DRIVERS_CORE_GPIO_HPP__

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>

#define GPIO_ADDR 0x41200000

// Offsets
// Set by Xilinx IP
#define CHAN1_VALUE_OFF    0
#define CHAN2_VALUE_OFF    8
#define CHAN1_DIR_OFF      4
#define CHAN2_DIR_OFF      12

/// Maximum bit index
/// For use on RedPitaya index < 7, but this is specific
/// I consider this device as a driver for the 
/// LogiCORE IP AXI GPIO
#define MAX_BIT_IDX 31

//> \description GPIO driver
class Gpio
{
  public:
    Gpio(Klib::DevMem& dev_mem_);
    ~Gpio();

    //> \description Open the device
    //> \io_type WRITE
    //> \param map_size Size of the device memory to be mapped
    //> \status ERROR_IF_NEG
    //> \on_error Cannot open GPIO device
    //> \flag AT_INIT
    int Open(uint32_t map_size = 16*4096);
    
    void Close();

    //> \description Set a bit
    //> \io_type WRITE
    //> \param index Position of the bit to set
    //> \param channel GPIO channel
    //> \status ERROR_IF_NEG
    int set_bit(uint32_t index, uint32_t channel);

    //> \description Erase a bit
    //> \io_type WRITE
    //> \param index Position of the bit to erase
    //> \param channel GPIO channel
    //> \status ERROR_IF_NEG
    int clear_bit(uint32_t index, uint32_t channel);

    //> \description Toggle a bit
    //> \io_type WRITE
    //> \param index Position of the bit to set
    //> \param channel GPIO channel
    //> \status ERROR_IF_NEG
    int toggle_bit(uint32_t index, uint32_t channel);

    //> \description Set a bit direction as input
    //> \io_type WRITE
    //> \param index Position of the bit to set
    //> \param channel GPIO channel
    //> \status ERROR_IF_NEG
    int set_as_input(uint32_t index, uint32_t channel);

    //> \description Set a bit direction as output
    //> \io_type WRITE
    //> \param index Position of the bit to set
    //> \param channel GPIO channel
    //> \status ERROR_IF_NEG
    int set_as_output(uint32_t index, uint32_t channel);

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
}; // class Gpio

#endif // __DRIVERS_CORE_GPIO_HPP__
