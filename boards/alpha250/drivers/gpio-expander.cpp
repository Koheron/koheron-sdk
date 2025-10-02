#include "./gpio-expander.hpp"

#include "server/context/context.hpp"

GpioExpander::GpioExpander(Context& ctx)
: i2c(ctx.i2c.get("i2c-0"))
{
    write_reg(0x4F, 2); // Port 0: push-pull, Port 1: open drain
    write_reg(0x42, 0);
    write_reg(0x43, 0);
    write_reg(Configuration_port_0, 0b11110000);
    write_reg(Configuration_port_1, 0b00000000);
}

void GpioExpander::set_led(uint8_t value) {
    write_reg(Output_port_1, ~value);
}

uint32_t GpioExpander::get_inputs() {
    uint8_t data = 0;
    read_reg(Input_port_0, data);
    return data;
}

void GpioExpander::set_user_ios(uint8_t value) {
    write_reg(Output_port_0, (value & 0xF));
}

int32_t GpioExpander::write_reg(uint8_t addr, uint8_t value) {
    std::array<uint8_t, 2> buff {addr, value};
    return i2c.write(i2c_address, buff);
}

int32_t GpioExpander::read_reg(uint8_t addr, uint8_t &data) {
    if (i2c.write(i2c_address, addr) < 0) {
        return -1;
    }
    if (i2c.read(i2c_address, data) < 0) {
        return -1;
    }
    return 1;
}