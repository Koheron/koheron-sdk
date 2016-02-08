/// Implementation of init_tasks.hpp
///
/// (c) Koheron

#include "init.hpp"

#include <cstdio>
#include <cstring>

extern "C" {
  #include <sys/socket.h>
  #include <sys/types.h>  
  #include <arpa/inet.h>
  #include <ifaddrs.h>
}

#include <drivers/wr_register.hpp>
#include <drivers/addresses.hpp>

Init::Init(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
{}

// http://stackoverflow.com/questions/20800319/how-to-get-my-ip-address-in-c-linux
void Init::run()
{ 
    struct ifaddrs *addrs;
    getifaddrs(&addrs);
    ifaddrs *tmp = addrs;

    // Turn all the leds ON
    Klib::MemMapID config = dev_mem.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
    Klib::WriteReg32(dev_mem.GetBaseAddr(config) + LED_OFF, 255);

    char interface[] = "eth0";

    while(tmp) {
        // Works only for IPv4 address
        if(tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {            
            struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
            int val = strcmp(tmp->ifa_name,interface);
            
            if(val != 0) {
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

