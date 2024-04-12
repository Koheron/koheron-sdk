/// Common commands for all Koheron ALPHA250-4 bitstreams
///
/// (c) Koheron

#ifndef __ALPHA250_4_DRIVERS_COMMON_HPP__
#define __ALPHA250_4_DRIVERS_COMMON_HPP__

#include <cstring>

extern "C" {
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <arpa/inet.h>
  #include <ifaddrs.h>
}

#include <context.hpp>
#include <boards/alpha250/drivers/gpio-expander.hpp>
#include <boards/alpha250/drivers/temperature-sensor.hpp>
#include <boards/alpha250/drivers/precision-dac.hpp>
#include <boards/alpha250/drivers/precision-adc.hpp>
#include "clock-generator.hpp"
#include "ltc2157.hpp"

class Common
{
  public:
    Common(Context& ctx_)
    : ctx(ctx_)
    , sts(ctx.mm.get<mem::status>())
    , clkgen(ctx.get<ClockGenerator>())
    , gpio(ctx.get<GpioExpander>())
    , ltc2157(ctx.get<Ltc2157>())
    , precisiondac(ctx.get<PrecisionDac>())
    , precisionadc(ctx.get<PrecisionAdc>())
    {}

    void set_led(uint32_t value) {
        gpio.set_led(value);
    }

    void init() {
        ctx.log<INFO>("Common - Initializing ...");
        clkgen.init();
        ltc2157.init();
        precisiondac.init();
        // ip_on_leds();
    };

    std::string get_instrument_config() {
        return CFG_JSON;
    }

    void ip_on_leds() {
        struct ifaddrs *addrs;
        getifaddrs(&addrs);
        ifaddrs *tmp = addrs;

        // Turn all the leds ON
        gpio.set_led(255);

        char interface[] = "eth0";

        while (tmp) {
            // Works only for IPv4 address
            if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wcast-align"
                struct sockaddr_in *pAddr = reinterpret_cast<struct sockaddr_in *>(tmp->ifa_addr);
                #pragma GCC diagnostic pop
                int val = strcmp(tmp->ifa_name, interface);

                if (val != 0) {
                    tmp = tmp->ifa_next;
                    continue;
                }

                ctx.log<INFO>("Interface %s found: %s\n", tmp->ifa_name, inet_ntoa(pAddr->sin_addr));
                uint32_t ip = htonl(pAddr->sin_addr.s_addr);

                // Write IP address
                set_led(ip);
                break;
            }

            tmp = tmp->ifa_next;
        }

        freeifaddrs(addrs);
    }

  private:
    Context& ctx;
    Memory<mem::status>& sts;
    ClockGenerator& clkgen;
    GpioExpander& gpio;
    Ltc2157& ltc2157;
    PrecisionDac& precisiondac;
    PrecisionAdc& precisionadc;
};

#endif // __ALPHA250_4_DRIVERS_COMMON_HPP__
