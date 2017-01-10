/// (c) Koheron

#ifndef __CONTEXT_HPP__
#define __CONTEXT_HPP__

#include <core/context_base.hpp>

#include <drivers/lib/memory_manager.hpp>
#include <drivers/lib/spi_dev.hpp>
#include <drivers/lib/i2c_dev.hpp>

#include "memory.hpp"

class Context : public ContextBase
{
  public:
    Context()
    : mm()
#ifdef CTX_HAS_SPI
    , spi(*this)
#endif
#ifdef CTX_HAS_I2C
    , i2c(*this)
#endif
    {}

    int init() {
        if (mm.open() < 0)
            return -1;
#ifdef CTX_HAS_SPI
        if (spi.init() < 0)
            return -1;
#endif
#ifdef CTX_HAS_I2C
        if (i2c.init() < 0)
            return -1;
#endif
        return 0;
    }

    MemoryManager mm;
#ifdef CTX_HAS_SPI
    SpiDev spi;
#endif
#ifdef CTX_HAS_I2C
    I2cDev i2c;
#endif
};

#endif // __CONTEXT_HPP__