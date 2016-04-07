/// (c) Koheron

#include "common.hpp"

#include <cstring>

extern "C" {
  #include <sys/socket.h>
  #include <sys/types.h>  
  #include <arpa/inet.h>
  #include <ifaddrs.h>
}

Common::Common(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
{
    status = CLOSED;
}

int Common::Open()
{
    if (status == CLOSED) {
        std::array<Klib::MemMapID, 2> ids = dev_mem.RequestMemoryMaps(mem_regions);

        if (dev_mem.CheckMapIDs(ids) < 0) {
            status = FAILED;
            return -1;
        }

        config_map = ids[0];
        status_map = ids[1];

        status = OPENED;
    }

    return 0;
}

std::array<uint32_t, BITSTREAM_ID_SIZE> Common::get_bitstream_id()
{
    for (uint32_t i=0; i<bitstream_id.size(); i++)
        bitstream_id[i] = Klib::ReadReg32(dev_mem.GetBaseAddr(status_map) 
                                          + BITSTREAM_ID_OFF + 4*i);

    return bitstream_id;
}

uint64_t Common::get_dna()
{
    uint64_t dna_low  = static_cast<uint64_t>(Klib::ReadReg32(dev_mem.GetBaseAddr(status_map) + DNA_OFF));
    uint64_t dna_high = static_cast<uint64_t>(Klib::ReadReg32(dev_mem.GetBaseAddr(status_map) + DNA_OFF + 4));

    return dna_low + (dna_high << 32);
}

void Common::set_led(uint32_t value)
{
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + LED_OFF, value);
}

uint32_t Common::get_led()
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(config_map) + LED_OFF);
}

// http://stackoverflow.com/questions/20800319/how-to-get-my-ip-address-in-c-linux
void Common::ip_on_leds()
{
    struct ifaddrs *addrs;
    getifaddrs(&addrs);
    ifaddrs *tmp = addrs;

    // Turn all the leds ON
    Klib::MemMapID config = dev_mem.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
    Klib::WriteReg32(dev_mem.GetBaseAddr(config) + LED_OFF, 255);

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

            // Interface found
            printf("Interface %s found: %s\n", 
                   tmp->ifa_name, inet_ntoa(pAddr->sin_addr));
            uint32_t ip = htonl(pAddr->sin_addr.s_addr);

            // Write IP address in FPGA memory
            // The 8 Least Significant Bits should be connected to the FPGA LEDs
            Klib::WriteReg32(dev_mem.GetBaseAddr(config) + LED_OFF, ip);

            break;
        }
        
        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);
    dev_mem.RmMemoryMap(config);
}