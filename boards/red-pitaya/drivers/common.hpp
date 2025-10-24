/// Common commands for all bitstreams
///
/// (c) Koheron

#ifndef __DRIVERS_COMMON_HPP__
#define __DRIVERS_COMMON_HPP__

#include "server/runtime/syslog.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/drivers/leds-control.hpp"

#include <cstdint>
#include <string>
#include <memory>

class Common
{
  public:
    Common()
    : leds(std::make_unique<LedsController>())
    , ctl(hw::get_memory<mem::control>())
    {
        leds->setter([&](uint32_t v){
            ctl.write<reg::led>(v);
        });
    }

    ~Common() {
        leds->stop_blink();
    }

    void set_led(uint32_t value) {
        leds->set_led(value);
    }

    void init() {
        leds->start_blink();
    };

    std::string get_instrument_config() {
        return CFG_JSON;
    }

    void ip_on_leds() {
        leds->ip_on_leds();
    }

  private:
    std::unique_ptr<LedsController> leds;
    hw::Memory<mem::control>& ctl;
};

#endif // __DRIVERS_COMMON_HPP__
