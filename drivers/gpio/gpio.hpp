/// GPIO driver
///
/// http://www.xilinx.com/support/documentation/ip_documentation/axi_gpio/v2_0/pg144-axi-gpio.pdf
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_GPIO_HPP__
#define __DRIVERS_CORE_GPIO_HPP__

#include <drivers/lib/dev_mem.hpp>

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
    Gpio(DevMem& dvm_)
    : dvm(dvm_)
    , gpio(dvm[GPIO_ID]) 
    {}

    void set_data(uint32_t channel, uint32_t value) {
        gpio.write_offset(get_value_offset(channel), value);
    }

    uint32_t get_data(uint32_t channel) {
        return gpio.read_offset(get_value_offset(channel));
    }

    // Bitwise operations
    void set_bit(uint32_t index, uint32_t channel) {
        if (index <= MAX_BIT_IDX)
            gpio.set_bit_offset(get_value_offset(channel), index);
    }

    void clear_bit(uint32_t index, uint32_t channel) {
        if (index <= MAX_BIT_IDX)
            gpio.clear_bit_offset(get_value_offset(channel), index);
    }

    void toggle_bit(uint32_t index, uint32_t channel) {
        if (index <= MAX_BIT_IDX)
            gpio.toggle_bit_offset(get_value_offset(channel), index);
    }

    bool read_bit(uint32_t index, uint32_t channel) {
        return gpio.read_bit_offset(get_value_offset(channel), index);
    }

    void set_as_input(uint32_t index, uint32_t channel) {
        if (index <= MAX_BIT_IDX)
            gpio.set_bit_offset(get_dir_offset(channel), index);
    }

    void set_as_output(uint32_t index, uint32_t channel) {
        if (index <= MAX_BIT_IDX)
            gpio.clear_bit_offset(get_dir_offset(channel), index);
    }

  private:
    DevMem& dvm;
    MemoryMap& gpio;

    int get_value_offset(uint32_t channel) {
        return (channel == 1 ? CHAN1_VALUE_OFF : CHAN2_VALUE_OFF);
    }

    int get_dir_offset(uint32_t channel) {
        return (channel == 1 ? CHAN1_DIR_OFF : CHAN2_DIR_OFF);
    }
}; // class Gpio

#endif // __DRIVERS_CORE_GPIO_HPP__
