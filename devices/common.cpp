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

Common::~Common()
{
    Close();
}

int Common::Open()
{
    if (status == CLOSED) {    
        // Initializes memory maps
        config_map = dev_mem.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);

        if (static_cast<int>(config_map) < 0) {
            status = FAILED;
            return -1;
        }

        status_map = dev_mem.AddMemoryMap(STATUS_ADDR, STATUS_RANGE);
        
        if (static_cast<int>(status_map) < 0) {
            status = FAILED;
            return -1;
        }

        status = OPENED;
    }

    return 0;
}

void Common::Close()
{
    if (status == OPENED) {
        dev_mem.RmMemoryMap(config_map);
        dev_mem.RmMemoryMap(status_map);
        status = CLOSED;
    }
}

std::array<uint32_t, BITSTREAM_ID_SIZE> Common::get_bitstream_id()
{
    for (uint32_t i=0; i<bitstream_id.size(); i++)
        bitstream_id[i] = Klib::ReadReg32(dev_mem.GetBaseAddr(status_map) 
                                          + BITSTREAM_ID_OFF + 4*i);

    return bitstream_id;
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