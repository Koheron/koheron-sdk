/// (c) Koheron

#ifndef __ALPHA_DRIVERS_GPIO_EXPANDER_HPP__
#define __ALPHA_DRIVERS_GPIO_EXPANDER_HPP__

#include <cstdint>
#include <array>

// http://www.nxp.com/documents/data_sheet/PCAL6416A.pdf
// PORT 0:
// P0_0: USER_IO0
// P0_1: USER_IO1
// P0_2: USER_IO2
// P0_3: USER_IO3
// P0_4: EN_ADC_FRONTEND
// P0_5: EN_DAC_FRONTEND
// P0_6: PLL_STATUS_HOLDOVER
// P0_7: PLL_STATUS_LD
// PORT 1:
// P1_0: USER_LED0
// P1_1: USER_LED1
// P1_2: USER_LED2
// P1_3: USER_LED3
// P1_4: USER_LED4
// P1_5: USER_LED5
// P1_6: USER_LED6
// P1_7: USER_LED7

class Context;
class I2cDev;

class GpioExpander
{
  public:
    GpioExpander(Context& ctx);

    void set_led(uint8_t value);
    uint32_t get_inputs();
    void set_user_ios(uint8_t value);

  private:
    static constexpr uint32_t i2c_address = 0b0100000;
    I2cDev& i2c;

    // Command addresses
    static constexpr uint8_t Input_port_0 = 0x0;
    static constexpr uint8_t Input_port_1 = 0x1;
    static constexpr uint8_t Output_port_0 = 0x2;
    static constexpr uint8_t Output_port_1 = 0x3;
    static constexpr uint8_t Configuration_port_0 = 0x6;
    static constexpr uint8_t Configuration_port_1 = 0x7;

    int32_t write_reg(uint8_t addr, uint8_t value);
    int32_t read_reg(uint8_t addr, uint8_t &data);
};

#endif // __ALPHA_DRIVERS_GPIO_EXPANDER_HPP__