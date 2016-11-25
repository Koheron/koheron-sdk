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
#include <drivers/init/init.hpp>

class Common
{
  public:
    Common(Context& ctx_)
    : ctx(ctx_)
    , cfg(ctx.mm.get<mem::config>())
    , sts(ctx.mm.get<mem::status>())
    {}

    auto& get_bitstream_id() {
        return sts.read_array<uint32_t, prm::bitstream_id_size, reg::bitstream_id>();
    }

    uint64_t get_dna() {
        return sts.read<reg::dna, uint64_t>();
    }

    void set_led(uint32_t value) {
        cfg.write<reg::led>(value);
    }

    uint32_t get_led() {
        return cfg.read<reg::led>();
    }

    void init() {
        ip_on_leds();
        ctx.get<Init>().load_settings();
    };

    void cfg_write(uint32_t offset, uint32_t value) {
        cfg.write_reg(offset, value);
    }

    uint32_t cfg_read(uint32_t offset) {
        return cfg.read_reg(offset);
    }

    auto& cfg_read_all() {
        return cfg.read_array<uint32_t, mem::config_range/4>();
    }

    auto& sts_read_all() {
        return sts.read_array<uint32_t, mem::status_range/4>();
    }

    uint32_t sts_read(uint32_t offset) {
        return sts.read_reg(offset);
    }

    std::string get_instrument_config() {
        return CFG_JSON;
    }

    void ip_on_leds() {
        struct ifaddrs *addrs;
        getifaddrs(&addrs);
        ifaddrs *tmp = addrs;

        // Turn all the leds ON
        cfg.write<reg::led>(255);

        char interface[] = "eth0";

        while (tmp) {
            // Works only for IPv4 address
            if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
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
                cfg.write_mask<reg::led, 0xFF>(ip);
                break;
            }

            tmp = tmp->ifa_next;
        }

        freeifaddrs(addrs);
    }

  private:
    Context& ctx;
    Memory<mem::config>& cfg;
    Memory<mem::status>& sts;
};

#endif // __DRIVERS_COMMON_HPP__
