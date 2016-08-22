/// (c) Koheron

#include "common.hpp"

#include <cstring>

extern "C" {
  #include <sys/socket.h>
  #include <sys/types.h>  
  #include <arpa/inet.h>
  #include <ifaddrs.h>
}

std::array<uint32_t, prm::bitstream_id_size> Common::get_bitstream_id()
{
    for (uint32_t i=0; i<bitstream_id.size(); i++)
        bitstream_id[i] = sts.read_offset(reg::bitstream_id + 4 * i);

    return bitstream_id;
}

uint64_t Common::get_dna()
{
    uint64_t dna_low  = static_cast<uint64_t>(sts.read<reg::dna>());
    uint64_t dna_high = static_cast<uint64_t>(sts.read<reg::dna + 4>());
    return dna_low + (dna_high << 32);
}

// http://stackoverflow.com/questions/20800319/how-to-get-my-ip-address-in-c-linux
void Common::ip_on_leds()
{
    struct ifaddrs *addrs;
    getifaddrs(&addrs);
    ifaddrs *tmp = addrs;

    // Turn all the leds ON
    cfg.write<LED_OFF>(255);

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
            cfg.write<LED_OFF>(ip);
            break;
        }
        
        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);
}
