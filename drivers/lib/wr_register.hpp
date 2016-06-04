/// Memory map devices and write/read registers.
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_WR_REGISTER_HPP__
#define __DRIVERS_CORE_WR_REGISTER_HPP__

#include <bitset>
#include <cstdint>

/// @namespace Klib
/// @brief Namespace of the Koheron library
namespace Klib {

// -- I/O access

/// Write a value in a 32 bits register
/// @addr Absolute address of the register to be written
/// @RegVal Value to be written (uint32)
inline void WriteReg32(uintptr_t addr, uint32_t reg_val)
{
    *(volatile uintptr_t *) addr = reg_val;
}

/// Write a value in a 32 bits register
/// @addr Absolute address of the register to be written
/// @reg_val Value to be written (bitset<32>)
inline void WriteReg32(uintptr_t addr, std::bitset<32> reg_val)
{
    *(volatile uintptr_t *) addr = reg_val.to_ulong();
}

/// Write a buffer of 32 bits registers
/// @addr Absolute address of the first register of the buffer
/// @data_ptr Pointer to the data to be written
/// @buff_size Number of data to write in the buffer
inline void WriteBuff32(uintptr_t addr, const uint32_t *data_ptr, 
                        uint32_t buff_size)
{
    for(uint32_t i=0; i < buff_size; i++) {
        WriteReg32(addr + sizeof(uint32_t) * i, data_ptr[i]);
    }
}

/// Read a value in a 32 bits register
/// @addr Absolute address of the register to be read
inline uint32_t ReadReg32(uintptr_t addr)
{
    return *(volatile uintptr_t *) addr;
}

// -- Bit manipulations
//
// http://stackoverflow.com/questions/47981/how-do-you-set-clear-and-toggle-a-single-bit-in-c-c

/// Set a bit in a 32 bits register
/// @addr Absolute address of the register
/// @index Index of the bit in the register
inline void SetBit(uintptr_t addr, uint32_t index)
{
    *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) | (1 << index);
}

/// Clear a bit in a 32 bits register
/// @addr Absolute address of the register
/// @index Index of the bit in the register
inline void ClearBit(uintptr_t addr, uint32_t index)
{
    *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) & ~(1 << index);
}

/// Toggle a bit in a 32 bits register
/// @addr Absolute address of the register
/// @index Index of the bit in the register
inline void ToggleBit(uintptr_t addr, uint32_t index)
{
    *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) ^ (1 << index);
}

/// Obtain the value of a bit
/// @addr Absolute address of the register
/// @index Index of the bit in the register
inline bool ReadBit(uintptr_t addr, uint32_t index)
{
    return *((volatile uintptr_t *) addr) & (1 << index);
}

// -- Masks

inline void MaskAnd(uintptr_t addr, uint32_t mask)
{
    *(volatile uintptr_t *) addr &= mask;
}

inline void MaskOr(uintptr_t addr, uint32_t mask)
{
    *(volatile uintptr_t *) addr |= mask;
}

}; // namespace Klib

#endif // __DRIVERS_CORE_WR_REGISTER_HPP__
