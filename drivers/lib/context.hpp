/// (c) Koheron

#ifndef __CONTEXT_HPP__
#define __CONTEXT_HPP__

#include <core/context_base.hpp>

#include <drivers/lib/memory_manager.hpp>
#include "memory.hpp"

class Context : public ContextBase
{
  public:
    Context()
    : mm()
    {}

    int init() {
        return mm.open();
    }

    MemoryManager mm;
};

#endif // __CONTEXT_HPP__