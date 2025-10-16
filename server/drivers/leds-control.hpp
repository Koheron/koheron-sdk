#ifndef __SERVER_DRIVERS_LEDS_CONTROL_HPP__
#define __SERVER_DRIVERS_LEDS_CONTROL_HPP__

#include <utility>
#include <cstdint>
#include <thread>
#include <atomic>
#include <cstring>
#include <chrono>
#include <functional>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

class LedsController {
  public:
    using Setter = std::function<void(uint32_t)>;

    LedsController() = default;

    ~LedsController() {
        stop_blink();
    }

    void setter(Setter f) {
        setter_ = std::move(f);
    }

    void set_led(uint32_t v) {
        if (setter_) {
            setter_(v);
        }
    }

    void all_on()  { set_led(255u); }
    void all_off() { set_led(0u);   }; 

    void ip_on_leds() {
        stop_blink();
        struct ifaddrs* addrs = nullptr;

        if (getifaddrs(&addrs) != 0 || !addrs) {
            return;
        }

        all_on();

        for (const char* want : {"end0", "eth0"}) {
            for (auto* it = addrs; it; it = it->ifa_next) {
                if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) {
                    continue;
                }

                if (std::strcmp(it->ifa_name, want) != 0) {
                    continue;
                }

                auto* pAddr = reinterpret_cast<sockaddr_in*>(it->ifa_addr);
                logf("ip_on_leds: Interface {} found: {}\n",
                     it->ifa_name, inet_ntoa(pAddr->sin_addr));
                uint32_t ip = htonl(pAddr->sin_addr.s_addr);
                set_led(ip);
                freeifaddrs(addrs);
                return;
            }
        }

        // Neither end0 nor eth0 had an IPv4; keep LEDs as-is
        freeifaddrs(addrs);
    }

    void start_blink() {
        if (blinker.joinable()) {
            return;
        }

        blinker_should_stop.store(false, std::memory_order_release);

        blinker = std::thread([this]{
            using namespace std::chrono;
            constexpr auto step = milliseconds(100);
            all_off();
            auto next_tick = std::chrono::steady_clock::now() + step;
            uint32_t pat = 0x01;

            while (!blinker_should_stop.load(std::memory_order_acquire)) {
                set_led(pat);
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

  private:
    Setter setter_;

    std::thread blinker;
    std::atomic<bool> blinker_should_stop{true}; // true = not running yet
};

#endif // __SERVER_DRIVERS_LEDS_CONTROL_HPP__