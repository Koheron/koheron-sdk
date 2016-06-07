/// Memory map devices
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_MEMORY_MAP_HPP__
#define __DRIVERS_CORE_MEMORY_MAP_HPP__

#include <cstdio>
#include <cstdint>

extern "C" {
    #include <unistd.h>
    #include <sys/mman.h>
}

/// @namespace Klib
/// @brief Namespace of the Koheron library
namespace Klib {

#define DEFAULT_MAP_SIZE 4096UL // = PAGE_SIZE
#define MAP_MASK(size) ((size) - 1)

/// @brief Memory map a device
class MemoryMap
{
  public:
    /// Build a memory map
    /// @phys_addr_ Physical base address of the device
    /// @size_ Map size in octets
    MemoryMap(int *fd_, uintptr_t phys_addr_, 
              uint32_t size_ = DEFAULT_MAP_SIZE, 
              int protection_ = READ_WRITE);

    ~MemoryMap();

    /// Close the memory map
    /// @return 0 if succeed, -1 else
    /// @bug Unmapping doesn't work sometimes
    int Unmap();

    int Resize(uint32_t length);

    int GetProtection() const {return protection;}
    int GetStatus() const {return status;}
    uintptr_t GetBaseAddr() const {return mapped_dev_base;}
    uint32_t MappedSize() const {return size;}
    uintptr_t PhysAddr() const {return phys_addr;}

    enum Status {
        MEMMAP_CLOSED,       ///< Memory map closed
        MEMMAP_OPENED,       ///< Memory map opened
        MEMMAP_CANNOT_UMMAP, ///< Memory map cannot be unmapped
        MEMMAP_FAILURE       ///< Failure at memory mapping
    };

    enum Permissions {
        READ_WRITE,
        READ_ONLY,
        WRITE_ONLY,
        permissions_num
    };

  private:
    int *fd;                    ///< /dev/mem file ID (Why is this a pointer ?)
    void *mapped_base;          ///< Map base address
    uintptr_t mapped_dev_base;  ///< Virtual memory base address of the device
    int status;                 ///< Status
    int protection;
    uint32_t size;              ///< Map size in bytes
    uintptr_t phys_addr;        ///< Physical address
};

}; // namespace Klib

#endif // __DRIVERS_CORE_MEMORY_MAP_HPP__
