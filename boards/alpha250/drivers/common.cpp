#include "./common.hpp"
#include "./clock-generator.hpp"
#include "./ltc2157.hpp"
#include "./ad9747.hpp"
#include "./precision-dac.hpp"
#include "./gpio-expander.hpp"
#include "./precision-adc.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/driver_manager.hpp"

#include <cstring>
#include <chrono>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

using services::require;

Common::Common()
: gpio(require<rt::DriverManager>().get<GpioExpander>())
, precisionadc(require<rt::DriverManager>().get<PrecisionAdc>()) // Initialize PrecisionADC
{}

Common::~Common() {
    stop_blink(); // in case ip_on_leds() was never called
}

void Common::set_led(uint32_t value) {
    gpio.set_led(value);
}

void Common::init() {
    log("Common: Initializing ...");
    start_blink();

    auto& dm = require<rt::DriverManager>();
    dm.get<ClockGenerator>().init();
    dm.get<Ltc2157>().init();
    dm.get<Ad9747>().init();
    dm.get<PrecisionDac>().init();
};

std::string Common::get_instrument_config() {
    return CFG_JSON;
}

void Common::ip_on_leds() {
    stop_blink();
    struct ifaddrs* addrs = nullptr;
    if (getifaddrs(&addrs) != 0 || !addrs) return;

    // Turn all the LEDs ON
    gpio.set_led(255);

    for (const char* want : {"end0", "eth0"}) {
        for (auto* it = addrs; it; it = it->ifa_next) {
            if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
            if (std::strcmp(it->ifa_name, want) != 0) continue;

            auto* pAddr = reinterpret_cast<sockaddr_in*>(it->ifa_addr);
            logf("ip_on_leds: Interface {} found: {}\n", it->ifa_name, inet_ntoa(pAddr->sin_addr));
            uint32_t ip = htonl(pAddr->sin_addr.s_addr);
            set_led(ip);
            freeifaddrs(addrs);
            return;
        }
    }

    // Neither end0 nor eth0 had an IPv4; keep LEDs as-is
    freeifaddrs(addrs);
}

void Common::start_blink() {
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

void Common::stop_blink() {
    blinker_should_stop.store(true, std::memory_order_release);
    if (blinker.joinable()) {
        blinker.join();
    }
}
