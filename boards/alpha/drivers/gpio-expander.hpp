/// (c) Koheron

#ifndef __DRIVERS_GPIO_EXPANDER_HPP__
#define __DRIVERS_GPIO_EXPANDER_HPP__

#include <context.hpp>

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

class GpioExpander
{
  public:
    GpioExpander(Context& ctx_)
    : ctx(ctx_)
    , i2c(ctx.i2c.get("i2c-0"))
    {
        write_reg(0x4F, 3);
        write_reg(0x42, 0);
        write_reg(0x43, 0);
        write_reg(Configuration_port_0, 0b00001111);
        write_reg(Configuration_port_1, 0b00000000);
    }

    void set_led(uint32_t value) {
        write_reg(Output_port_1, uint8_t(~value));
    }

    uint32_t get_inputs() {
        uint8_t data = 0;
        read_reg(Input_port_0, data);
        return data;
    }

  private:
    static constexpr uint32_t i2c_address = 0b010'0000;
    Context& ctx;
    I2cDev& i2c;

    // Command addresses
    static constexpr uint8_t Input_port_0 = 0x0;
    static constexpr uint8_t Input_port_1 = 0x1;
    static constexpr uint8_t Output_port_0 = 0x2;
    static constexpr uint8_t Output_port_1 = 0x3;
    static constexpr uint8_t Configuration_port_0 = 0x6;
    static constexpr uint8_t Configuration_port_1 = 0x7;

    int32_t write_reg(uint8_t addr, uint8_t value) {
        std::array<uint8_t, 2> buff {addr, value};
        return i2c.write(i2c_address, buff);
    }

    int32_t read_reg(uint8_t addr, uint8_t &data) {
        if (i2c.write(i2c_address, addr) < 0) {
            return -1;
        }
        if (i2c.read(i2c_address, data) < 0) {
            return -1;
        }
        return 1;
    }

};

#endif // __DRIVERS_GPIO_EXPANDER_HPP__