/// Tasks to be performed at init. Callable via the CLI.
///
/// (c) Koheron

#ifndef __MISC_INIT_HPP__
#define __MISC_INIT_HPP__

#include <drivers/dev_mem.hpp>

//> \description Operations to be performed at init. Callable via the CLI.
class Init
{
  public:
    Init(Klib::DevMem& dev_mem_);

    //> \io_type WRITE
    void run();
    
  private:
    Klib::DevMem& dev_mem;
};

#endif // __MISC_INIT_HPP__
