#include "./common.hpp"
#include "./clock-generator.hpp"
#include "./ltc2157.hpp"
#include "./ad9747.hpp"
#include "./precision-dac.hpp"
#include "./gpio-expander.hpp"
#include "./precision-adc.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/drivers/leds-control.hpp"

#include <cstring>
#include <chrono>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

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

void Common::start_blink() {
    leds->start_blink();
}

void Common::stop_blink() {
    leds->stop_blink();
}
