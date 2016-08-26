/// Tests for the memory API
///
/// (c) Koheron

#ifndef __DRIVERS_TEST_MEMORY_HPP__
#define __DRIVERS_TEST_MEMORY_HPP__

#include <cmath>

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

    // Write/read single registers

    bool write_read_u32() {
        ram.write<0>(42);
        ASSERT(ram.read<0>() == 42)

        ram.write<0>(4294967295);
        ASSERT(ram.read<0>() == 4294967295)

        return true;
    }

    bool write_read_reg_u32(uint32_t offset) {
        ram.write_reg(offset, 42);
        ASSERT(ram.read_reg(offset) == 42)

        ram.write_reg(offset, 4294967295);
        ASSERT(ram.read_reg(offset) == 4294967295)

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

    bool write_read_reg_i16(uint32_t offset) {
        ram.write_reg<int16_t>(offset, -42);
        ASSERT(ram.read_reg<int16_t>(offset) == -42)
        ASSERT(ram.read_reg<uint16_t>(offset) != -42)

        ram.write_reg<int16_t>(offset, -32767);
        ASSERT(ram.read_reg<int16_t>(offset) == -32767)

        ram.write_reg<int16_t>(offset, 32767);
        ASSERT(ram.read_reg<int16_t>(offset) == 32767)

        return true;
    }

    bool write_read_float() {
        ram.write<0, float>(3.1415926535897);
        ASSERT(fabs(ram.read<0, float>() - 3.1415926535897) < 1E-6)

        return true;
    }

    bool write_read_reg_float(uint32_t offset) {
        ram.write_reg<float>(offset, 3.1415926535897);
        ASSERT(fabs(ram.read_reg<float>(offset) - 3.1415926535897) < 1E-6)

        return true;
    }

    // Write/read consecutive registers

    bool write_read_u32_array() {
        std::array<uint32_t, 2048> arr;

        for (size_t i=0; i<arr.size(); i++)
            arr[i] = i * i;

        ram.write_array<uint32_t, 2048, 0>(arr);

        auto& arr_read = ram.read_array<uint32_t, 2048, 0>();

        for (size_t i=0; i<arr_read.size(); i++)
            ASSERT(arr_read[i] == i * i)

        return true;
    }

    bool write_read_reg_u32_array(uint32_t offset) {
        std::array<uint32_t, 2048> arr;

        for (size_t i=0; i<arr.size(); i++)
            arr[i] = i * i;

        ram.write_reg_array(offset, arr);

        auto& arr_read = ram.read_reg_array<uint32_t, 2048>(offset);

        for (size_t i=0; i<arr_read.size(); i++)
            ASSERT(arr_read[i] == i * i)

        return true;
    }

    bool write_read_float_array() {
        std::array<float, 2048> arr;

        for (size_t i=0; i<arr.size(); i++)
            arr[i] = log(static_cast<float>(i + 1));

        ram.write_array<float, 2048, 0>(arr);

        auto& arr_read = ram.read_array<float, 2048, 0>();

        for (size_t i=0; i<arr_read.size(); i++)
            ASSERT(fabs(arr_read[i] - log(static_cast<float>(i + 1))) < 1E-6)

        return true;
    }

    bool write_read_reg_float_array(uint32_t offset) {
        std::array<float, 2048> arr;

        for (size_t i=0; i<arr.size(); i++)
            arr[i] = log(static_cast<float>(i + 1));

        ram.write_reg_array(offset, arr);

        auto& arr_read = ram.read_reg_array<float, 2048>(offset);

        for (size_t i=0; i<arr_read.size(); i++)
            ASSERT(fabs(arr_read[i] - log(static_cast<float>(i + 1))) < 1E-6)

        return true;
    }

    // Low-level interfaces

    bool set_get_ptr_u32() {
        constexpr size_t len = 2048;
        uint32_t buffer[len];

        for (size_t i=0; i<len; i++)
            buffer[i] = i * i;

        ram.set_ptr<uint32_t, 0>(buffer, len);

        uint32_t *ram_ptr = ram.get_ptr<uint32_t, 0>();

        for (size_t i=0; i<len; i++)
            ASSERT(ram_ptr[i] == i * i)

        return true;
    }

    bool set_get_reg_ptr_u32(uint32_t offset) {
        constexpr size_t len = 2048;
        uint32_t buffer[len];

        for (size_t i=0; i<len; i++)
            buffer[i] = i * i;

        ram.set_reg_ptr<uint32_t>(offset, buffer, len);

        uint32_t *ram_ptr = ram.get_reg_ptr<uint32_t>(offset);

        for (size_t i=0; i<len; i++)
            ASSERT(ram_ptr[i] == i * i)

        return true;
    }

    bool set_get_ptr_float() {
        constexpr size_t len = 2048;
        float buffer[len];

        for (size_t i=0; i<len; i++)
            buffer[i] = sin(static_cast<float>(i));

        ram.set_ptr<float, 0>(buffer, len);

        float *ram_ptr = ram.get_ptr<float, 0>();

        for (size_t i=0; i<len; i++)
            ASSERT(fabs(ram_ptr[i] - sin(static_cast<float>(i))) < 1E-6)

        return true;
    }

    bool set_get_reg_ptr_float(uint32_t offset) {
        constexpr size_t len = 2048;
        float buffer[len];

        for (size_t i=0; i<len; i++)
            buffer[i] = sin(static_cast<float>(i));

        ram.set_reg_ptr<float>(offset, buffer, len);

        float *ram_ptr = ram.get_reg_ptr<float>(offset);

        for (size_t i=0; i<len; i++)
            ASSERT(fabs(ram_ptr[i] - sin(static_cast<float>(i))) < 1E-6)

        return true;
    }

    // Bit manipulations

    bool set_clear_bit() {
        ram.set_bit<0, 14>();
        ASSERT(ram.read_bit<0, 14>())
        ram.clear_bit<0, 14>();
        ASSERT(!ram.read_bit<0, 14>())
        return true;
    }

    bool set_clear_reg_bit(uint32_t offset, uint32_t index) {
        ram.set_bit_reg(offset, index);
        ASSERT(ram.read_bit_reg(offset, index))
        ram.clear_bit_reg(offset, index);
        ASSERT(!ram.read_bit_reg(offset, index))
        return true;
    }

    bool toggle_bit() {
        bool v0 = ram.read_bit<0, 14>();
        ram.toggle_bit<0, 14>();
        ASSERT(ram.read_bit<0, 14>() == !v0)
        return true;
    }

    bool toggle_reg_bit(uint32_t offset, uint32_t index) {
        bool v0 = ram.read_bit_reg(offset, index);
        ram.toggle_bit_reg(offset, index);
        ASSERT(ram.read_bit_reg(offset, index) == !v0)
        return true;
    }

    bool read_write_bit() {
        ram.write_bit<0, 10>(true);
        ASSERT(ram.read_bit<0, 10>())
        ram.write_bit<0, 12>(false);
        ASSERT(!ram.read_bit<0, 12>())
        return true;
    }

    bool read_write_reg_bit(uint32_t offset, uint32_t index) {
        ram.write_bit_reg(offset, index, true);
        ASSERT(ram.read_bit_reg(offset, index))
        ram.write_bit_reg(offset, index, false);
        ASSERT(!ram.read_bit_reg(offset, index))
        return true;
    }

  private:
    Memory<mem::rambuf>& ram;
};

#endif // __DRIVERS_TEST_MEMORY_HPP__