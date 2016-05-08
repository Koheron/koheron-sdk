/// Init commands for all bitstreams
///
/// (c) Koheron

#ifndef __DRIVERS_INIT_HPP__
#define __DRIVERS_INIT_HPP__

#include <drivers/lib/dev_mem.hpp>

class Init
{
  public:
    Init(Klib::DevMem& dev_mem_) {};

    void load_settings() {};
};

#endif // __DRIVERS_INIT_HPP__