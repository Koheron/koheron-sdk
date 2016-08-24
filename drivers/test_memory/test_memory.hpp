/// Tests for the memory API
///
/// (c) Koheron

#ifndef __DRIVERS_TEST_MEMORY_HPP__
#define __DRIVERS_TEST_MEMORY_HPP__

#include <drivers/lib/memory_manager.hpp>
#include <drivers/memory.hpp>

#define ASSERT(...) \
    if (!(__VA_ARGS__)) return false;

class TestMemory
{
  public:
    TestMemory(MemoryManager& mm)
    : ram(mm.get<mem::rambuf>())
    {}

    bool write_read_u32() {
        ram.write<0>(42);
        ASSERT(ram.read<0>() == 42)

        ram.write<0>(4294967295);
        ASSERT(ram.read<0>() == 4294967295)

        return true;
    }

    bool write_read_i16() {
        ram.write<0, int16_t>(-42);
        ASSERT(ram.read<0, int16_t>() == -42)
        ASSERT(ram.read<0, uint16_t>() != -42)

        ram.write<0, int16_t>(-32767);
        ASSERT(ram.read<0, int16_t>() == -32767)

        ram.write<0, int16_t>(32767);
        ASSERT(ram.read<0, int16_t>() == 32767)

        return true;
    }

  private:
    MemoryMap<mem::rambuf>& ram;
};

#endif // __DRIVERS_TEST_MEMORY_HPP__