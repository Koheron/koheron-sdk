/// Led Blinker driver
///
/// (c) Koheron

#ifndef __DRIVERS_LED_BLINKER_HPP__
#define __DRIVERS_LED_BLINKER_HPP__

#include <context.hpp>
#include <drivers/memory.hpp>

class LedBlinker
{
  public:
    LedBlinker(Context& ct)
    : cfg(ct.mm.get<mem::config>())
    , sts(ct.mm.get<mem::status>())
    {}

    void set_led(uint32_t led_value) {
        cfg.write<reg::led>(led_value);
    }

    uint32_t get_forty_two() {
        return sts.read<reg::forty_two>();
    }

  private:
    Memory<mem::config>& cfg;
    Memory<mem::status>& sts;
};

#endif // __DRIVERS_LED_BLINKER_HPP__
