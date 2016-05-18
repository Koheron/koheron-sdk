/// GPIO driver
///
/// http://www.xilinx.com/support/documentation/ip_documentation/axi_gpio/v2_0/pg144-axi-gpio.pdf
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_GPIO_HPP__
#define __DRIVERS_CORE_GPIO_HPP__

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/wr_register.hpp>

#define GPIO_ADDR          0x41200000
#define GPIO_RANGE         65536

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

class Gpio
{
  public:
    Gpio(Klib::DevMem& dev_mem_);

    int Open();
    void set_data(uint32_t channel, uint32_t value);
    uint32_t get_data(uint32_t channel);

    // Bitwise operations
    void set_bit(uint32_t index, uint32_t channel);
    void clear_bit(uint32_t index, uint32_t channel);
    void toggle_bit(uint32_t index, uint32_t channel);
    void set_as_input(uint32_t index, uint32_t channel);
    void set_as_output(uint32_t index, uint32_t channel);

    enum Status {
        CLOSED,
        OPENED,
        FAILED
    };

    #pragma tcp-server is_failed
    bool IsFailed() const {return status == FAILED;}

  private:
    Klib::DevMem& dev_mem;

    int status;

    // Memory maps IDs:
    Klib::MemMapID dev_num;
}; // class Gpio

#endif // __DRIVERS_CORE_GPIO_HPP__
