/// Tasks to be performed at init. Callable via the CLI.
///
/// (c) Koheron

#ifndef __MISC_INIT_HPP__
#define __MISC_INIT_HPP__

#include "dev_mem.hpp"

//> \description Task to be performed at init. Callable via the CLI.
class InitTasks
{
  public:
    InitTasks(Klib::DevMem& dev_mem_);
    
    //> \description Display IP address last number onto the board LEDs
    //> \io_type WRITE
    void show_ip_on_leds(uint32_t leds_addr);
    
  private:
    Klib::DevMem& dev_mem;
};

#endif // __MISC_INIT_HPP__
