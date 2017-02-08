/// (c) Koheron

#ifndef __CONTEXT_HPP__
#define __CONTEXT_HPP__

#include <core/context_base.hpp>

#include <drivers/context/memory_manager.hpp>
#include <drivers/context/spi_dev.hpp>
#include <drivers/context/i2c_dev.hpp>

#include "memory.hpp"

class Context : public ContextBase
{
  public:
    Context()
    : mm()
    , spi(*this)
    , i2c(*this)
    {}

    int init() {
        if (mm.open() < 0  ||
            spi.init() < 0 ||
            i2c.init() < 0)
            return -1;

        return 0;
    }

    MemoryManager mm;
    SpiManager spi;
    I2cManager i2c;
};

#endif // __CONTEXT_HPP__