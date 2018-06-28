/// Led Blinker driver
///
/// (c) Koheron

#ifndef __DRIVERS_LED_BLINKER_HPP__
#define __DRIVERS_LED_BLINKER_HPP__

#include <context.hpp>

class LedBlinker
{
  public:
    LedBlinker(Context& ctx)
    : ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    {}

    void set_leds(uint32_t led_value) {
        ctl.write<reg::led>(led_value);
    }

    uint32_t get_leds() {
        return ctl.read<reg::led>();
    }

    void set_led(uint32_t index, bool status) {
        ctl.write_bit_reg(reg::led, index, status);
    }

  private:
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
};

#endif // __DRIVERS_LED_BLINKER_HPP__
