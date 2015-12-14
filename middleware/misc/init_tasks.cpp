/// Implementation of init_tasks.hpp
///
/// (c) Koheron

#include "init_tasks.hpp"

#include <cstdio>
#include <cstring>

extern "C" {
  #include <sys/socket.h>
  #include <sys/types.h>  
  #include <arpa/inet.h>
  #include <ifaddrs.h>
}

#include "../drivers/core/wr_register.hpp"

InitTasks::InitTasks(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
{}

#define MAP_SIZE 4096
#define LED_OFFSET 0x0

// http://stackoverflow.com/questions/20800319/how-to-get-my-ip-address-in-c-linux
void InitTasks::show_ip_on_leds(uint32_t leds_addr)
{   
    struct ifaddrs *addrs;
    getifaddrs(&addrs);
    ifaddrs *tmp = addrs;

    // Turn all the leds ON
    Klib::MemMapID dev_num = dev_mem.AddMemoryMap(leds_addr, 16*MAP_SIZE);
    Klib::WriteReg32(dev_mem.GetBaseAddr(dev_num) + LED_OFFSET, 255);

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
            Klib::WriteReg32(dev_mem.GetBaseAddr(dev_num) + LED_OFFSET, ip);

            break;
        }
        
        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);
    dev_mem.RmMemoryMap(dev_num);
}
