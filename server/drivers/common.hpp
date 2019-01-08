/// Common commands for all bitstreams
///
/// (c) Koheron

#ifndef __DRIVERS_COMMON_HPP__
#define __DRIVERS_COMMON_HPP__

#include <cstring>
#include <array>

extern "C" {
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <arpa/inet.h>
  #include <ifaddrs.h>
}

#include <context.hpp>

class Common
{
  public:
    Common(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    {}

    uint64_t get_dna() {
        return sts.read<reg::dna, uint64_t>();
    }

    void set_led(uint32_t value) {
        ctl.write<reg::led>(value);
    }

    uint32_t get_led() {
        return ctl.read<reg::led>();
    }

    void init() {
        //ip_on_leds();
    };

    std::string get_instrument_config() {
        return CFG_JSON;
    }

    void ip_on_leds() {
        struct ifaddrs *addrs;
        getifaddrs(&addrs);
        ifaddrs *tmp = addrs;

        // Turn all the leds ON
        ctl.write<reg::led>(255);

        char interface[] = "eth0";

        while (tmp) {
            // Works only for IPv4 address
            if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wcast-align"
                struct sockaddr_in *pAddr = reinterpret_cast<struct sockaddr_in *>(tmp->ifa_addr);
                #pragma GCC diagnostic pop
                int val = strcmp(tmp->ifa_name,interface);

                if (val != 0) {
                    tmp = tmp->ifa_next;
                    continue;
                }

                printf("Interface %s found: %s\n",
                       tmp->ifa_name, inet_ntoa(pAddr->sin_addr));
                uint32_t ip = htonl(pAddr->sin_addr.s_addr);

                // Write IP address in FPGA memory
                // The 8 Least Significant Bits should be connected to the FPGA LEDs
                ctl.write_mask<reg::led, 0xFF>(ip);
                break;
            }

            tmp = tmp->ifa_next;
        }

        freeifaddrs(addrs);
    }

  private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
};

#endif // __DRIVERS_COMMON_HPP__
