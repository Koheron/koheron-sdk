#include "./common.hpp"
#include "./clock-generator.hpp"
#include "./ltc2387.hpp"
#include "./precision-dac.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/drivers/leds-control.hpp"
#include "boards/alpha250/drivers/gpio-expander.hpp"
#include "boards/alpha250/drivers/ad9747.hpp"
#include "boards/alpha250/drivers/temperature-sensor.hpp"

Common::Common()
: leds(std::make_unique<LedsController>())
{
    leds->setter([&](uint32_t v){
        rt::get_driver<GpioExpander>().set_led(v);
    });
}

Common::~Common() {
    leds->stop_blink();
}

void Common::set_led(uint32_t value) {
    leds->set_led(value);
}

void Common::init() {
    log("Common: Initializing ...\n");
    leds->start_blink();

    rt::get_driver<ClockGenerator>().init(); // Clock generator must be initialized before enabling LT2387 ADC
    rt::get_driver<Ltc2387>().init();
    rt::get_driver<Ad9747>().init();
    rt::get_driver<PrecisionDac>().init();
    adp5071_sync(0, 1); // By default synchronization disable. ADP5071 frequency set to 2.4 MHz.
};

void Common::ip_on_leds() {
    leds->ip_on_leds();
}

void Common::adp5071_sync(bool enable, bool state_out) {
    auto& ctl = hw::get_memory<mem::control>();

    if (enable) {
        ctl.set_bit<reg::adp5071_sync, 0>();
    } else {
        ctl.clear_bit<reg::adp5071_sync, 0>();

        // State out only applies when sync is disabled
        if (state_out) {
            ctl.set_bit<reg::adp5071_sync, 1>();
        } else {
            ctl.clear_bit<reg::adp5071_sync, 1>();
        }
    }
}

std::string Common::get_instrument_config() {
    return CFG_JSON;
}