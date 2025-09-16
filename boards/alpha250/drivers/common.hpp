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
#include "gpio-expander.hpp"
#include "clock-generator.hpp"
#include "ltc2157.hpp"
#include "ad9747.hpp"
#include "temperature-sensor.hpp"
#include "precision-dac.hpp"
#include "precision-adc.hpp"

class Common
{
  public:
    Common(Context& ctx_)
    : ctx(ctx_)
    , clkgen(ctx.get<ClockGenerator>())
    , gpio(ctx.get<GpioExpander>())
    , ltc2157(ctx.get<Ltc2157>())
    , ad9747(ctx.get<Ad9747>())
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
        ad9747.init();
        precisiondac.init();
        // ip_on_leds();
    };

    std::string get_instrument_config() {
        return CFG_JSON;
    }

    void ip_on_leds() {
        struct ifaddrs* addrs = nullptr;
        if (getifaddrs(&addrs) != 0 || !addrs) return;

        // Turn all the LEDs ON
        gpio.set_led(255);

        const char* preferred[] = {"end0", "eth0"};
        for (const char* want : preferred) {
            for (auto* it = addrs; it; it = it->ifa_next) {
                if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
                if (std::strcmp(it->ifa_name, want) != 0) continue;

                auto* pAddr = reinterpret_cast<sockaddr_in*>(it->ifa_addr);
                ctx.logf<INFO>("ip_on_leds: Interface {} found: {}\n", it->ifa_name, inet_ntoa(pAddr->sin_addr));
                uint32_t ip = htonl(pAddr->sin_addr.s_addr);
                set_led(ip);
                freeifaddrs(addrs);
                return;
            }
        }

        // Neither end0 nor eth0 had an IPv4; keep LEDs as-is (optional: log)
        // ctx.log<INFO>("No IPv4 on end0/eth0\n");
        freeifaddrs(addrs);
    }

  private:
    Context& ctx;
    ClockGenerator& clkgen;
    GpioExpander& gpio;
    Ltc2157& ltc2157;
    Ad9747& ad9747;
    PrecisionDac& precisiondac;
    PrecisionAdc& precisionadc;
};

#endif // __ALPHA_DRIVERS_COMMON_HPP__
