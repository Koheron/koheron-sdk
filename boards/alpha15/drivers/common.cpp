#include "./common.hpp"
#include "./clock-generator.hpp"
#include "./ltc2387.hpp"
#include "./precision-dac.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/drivers_manager.hpp"
#include "server/hardware/memory_manager.hpp"
#include "boards/alpha250/drivers/gpio-expander.hpp"
#include "boards/alpha250/drivers/ad9747.hpp"
#include "boards/alpha250/drivers/temperature-sensor.hpp"

#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

using services::require;

Common::Common()
: gpio(require<rt::DriverManager>().get<GpioExpander>())
{}

Common::~Common() {
    stop_blink();
}

void Common::set_led(uint32_t value) {
    gpio.set_led(value);
}

void Common::init() {
    log("Common: Initializing ...\n");
    start_blink();
    auto& dm = require<rt::DriverManager>();
    dm.get<ClockGenerator>().init(); // Clock generator must be initialized before enabling LT2387 ADC
    dm.get<Ltc2387>().init();
    dm.get<Ad9747>().init();
    dm.get<PrecisionDac>().init();
    adp5071_sync(0, 1); // By default synchronization disable. ADP5071 frequency set to 2.4 MHz.
    // ip_on_leds();
};

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
            logf("Interface {} found: {}\n", it->ifa_name, inet_ntoa(pAddr->sin_addr));
            uint32_t ip = htonl(pAddr->sin_addr.s_addr);
            set_led(ip);
            freeifaddrs(addrs);
            return;
        }
    }

    // Neither end0 nor eth0 had an IPv4; keep LEDs as-is
    freeifaddrs(addrs);
}

void Common::adp5071_sync(bool enable, bool state_out) {
    auto& ctl = require<hw::MemoryManager>().get<mem::control>();

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