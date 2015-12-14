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
    /// @brief Build a memory map
    /// @dev_addr Physical base address of the device (XXX intptr_t better ?)
    /// @size_ Map size in octets
    MemoryMap(int *fd_, uint32_t dev_addr, uint32_t size_ = DEFAULT_MAP_SIZE);

    ~MemoryMap();

    /// @brief Close the memory map
    /// @return 0 if succeed, -1 else
    /// @bug Unmapping doesn't work sometimes
    int Unmap();

    /// @brief Return the status
    inline int GetStatus() const {return status;}

    /// @brief Return the virtual memory base address of the device
    inline uint32_t GetBaseAddr() const {return mapped_dev_base;}

    /// @brief Return the mapped size in octets
    inline uint32_t MappedSize() const {return size;}
	
    enum Status {
        MEMMAP_CLOSED,       ///< Memory map closed
        MEMMAP_OPENED,       ///< Memory map opened
        MEMMAP_CANNOT_UMMAP, ///< Memory map cannot be unmapped
        MEMMAP_FAILURE       ///< Failure at memory mapping
    };

private:
    int *fd;                    ///< /dev/mem file ID (Why is this a pointer ?)
    void* mapped_base;          ///< Map base address
    uint32_t mapped_dev_base;   ///< Device base address
    int status;                 ///< Status
    uint32_t size;              ///< Map size
};

}; // namespace Klib

#endif // __DRIVERS_CORE_MEMORY_MAP_HPP__
