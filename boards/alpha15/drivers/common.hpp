/// Common commands for all Koheron Alpha bitstreams
///
/// (c) Koheron

#ifndef __ALPHA_DRIVERS_COMMON_HPP__
#define __ALPHA_DRIVERS_COMMON_HPP__

#include <cstring>

extern "C" {
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <arpa/inet.h>
  #include <ifaddrs.h>
}

#include <context.hpp>
#include <boards/alpha250/drivers/gpio-expander.hpp>
#include <boards/alpha250/drivers/ad9747.hpp>
#include <boards/alpha250/drivers/temperature-sensor.hpp>
#include <boards/alpha250/drivers/precision-dac.hpp>

#include "clock-generator.hpp"
#include "ltc2387.hpp"

class Common
{
  public:
    Common(Context& ctx_)
    : ctx(ctx_)
    , clkgen(ctx.get<ClockGenerator>())
    , gpio(ctx.get<GpioExpander>())
    , ltc2387(ctx.get<Ltc2387>())
    , ad9747(ctx.get<Ad9747>())
    , precisiondac(ctx.get<PrecisionDac>())
    , ctl(ctx.mm.get<mem::control>())
    {}

    uint64_t get_dna() {
        return ctx.mm.get<mem::status>().read<reg::dna, uint64_t>();
    }

    void set_led(uint32_t value) {
        gpio.set_led(value);
    }

    void init() {
        ctx.log<INFO>("Common - Initializing ...");
        clkgen.init(); // Clock generator must be initialized before enabling LT2387 ADC
        ltc2387.init();
        ad9747.init();
        precisiondac.init();
        adp5071_sync(0, 1); // By default synchronization disable. ADP5071 frequency set to 2.4 MHz.
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

    void adp5071_sync(bool enable, bool state_out) {
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

  private:
    Context& ctx;
    ClockGenerator& clkgen;
    GpioExpander& gpio;
    Ltc2387& ltc2387;
    Ad9747& ad9747;
    PrecisionDac& precisiondac;
    Memory<mem::control>& ctl;
};

#endif // __ALPHA_DRIVERS_COMMON_HPP__
