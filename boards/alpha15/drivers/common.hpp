/// Common commands for all Koheron Alpha bitstreams
///
/// (c) Koheron

#ifndef __ALPHA_DRIVERS_COMMON_HPP__
#define __ALPHA_DRIVERS_COMMON_HPP__


#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>

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

    ~Common() {
        stop_blink();
    }

    void set_led(uint32_t value) {
        gpio.set_led(value);
    }

    void init() {
        ctx.log<INFO>("Common - Initializing ...");
        start_blink();
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
        stop_blink();
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
                ctx.log<INFO>("Interface %s found: %s\n", it->ifa_name, inet_ntoa(pAddr->sin_addr));
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

    void start_blink() {
        if (blinker.joinable()) {
            return;
        }

        blinker_should_stop.store(false, std::memory_order_release);

        blinker = std::thread([this]{
            using namespace std::chrono;
            constexpr auto step = milliseconds(100);
            gpio.set_led(0x00);
            auto next_tick = std::chrono::steady_clock::now() + step;
            uint32_t pat = 0x01;

            while (!blinker_should_stop.load(std::memory_order_acquire)) {
                gpio.set_led(pat);
                std::this_thread::sleep_until(next_tick);
                next_tick += step;

                pat = (pat == 0x80) ? 0x01 : ((pat << 1) & 0xFF);
            }
        });
    }

    void stop_blink() {
        blinker_should_stop.store(true, std::memory_order_release);
        if (blinker.joinable()) {
            blinker.join();
        }
    }

    Context& ctx;
    ClockGenerator& clkgen;
    GpioExpander& gpio;
    Ltc2387& ltc2387;
    Ad9747& ad9747;
    PrecisionDac& precisiondac;
    Memory<mem::control>& ctl;
    std::thread blinker;
    std::atomic<bool> blinker_should_stop{true}; // true = not running yet

};

#endif // __ALPHA_DRIVERS_COMMON_HPP__
