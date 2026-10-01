#include "./common.hpp"
#include "./clock-generator.hpp"
#include "./ltc2157.hpp"
#include "./ad9747.hpp"
#include "./precision-dac.hpp"
#include "./gpio-expander.hpp"
#include "./precision-adc.hpp"
#include "./spi-config.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/drivers/leds-control.hpp"

Common::Common()
: leds(std::make_unique<LedsController>())
, precisionadc(rt::get_driver<PrecisionAdc>()) // Initialize PrecisionADC
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
    log("Common: Initializing ...");
    leds->start_blink();

    services::provide<SpiConfig>();

    rt::get_driver<ClockGenerator>().init();
    rt::get_driver<Ltc2157>().init();
    rt::get_driver<Ad9747>().init();
    rt::get_driver<PrecisionDac>().init();
};

std::string Common::get_instrument_config() {
    return CFG_JSON;
}

void Common::ip_on_leds() {
    leds->ip_on_leds();
}