/// Common commands for all bitstreams
///
/// (c) Koheron

#ifndef __DRIVERS_COMMON_HPP__
#define __DRIVERS_COMMON_HPP__

#include <cstring>
#include <array>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include <context.hpp>
#include <bram.hpp>

class Common
{
  public:
    Common(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , bram(ctx.get<Bram>())
    {}

    void set_led(uint32_t value) {
        ctl.write<reg::led>(value);
    }

    uint32_t get_led() {
        return ctl.read<reg::led>();
    }

    void init() {
        ip_on_leds();
        ctx.log<INFO>("Start Bram test\n");
        //bram.test();
    };

    std::string get_instrument_config() {
        return CFG_JSON;
    }

    void ip_on_leds() {
        struct ifaddrs* addrs = nullptr;
        if (getifaddrs(&addrs) != 0 || !addrs) return;

        // Turn all the LEDs ON
        set_led(255);

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
        ctx.log<INFO>("No IPv4 on end0/eth0\n");
        freeifaddrs(addrs);
    }

  private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Bram& bram;

};

#endif // __DRIVERS_COMMON_HPP__
