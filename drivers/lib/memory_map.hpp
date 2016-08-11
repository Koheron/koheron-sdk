/// Memory map devices
///
/// (c) Koheron

#ifndef __DRIVERS_LIB_MEMORY_MAP_HPP__
#define __DRIVERS_LIB_MEMORY_MAP_HPP__

#include <cstdio>
#include <cstdint>
#include <tuple>

extern "C" {
    #include <unistd.h>
    #include <sys/mman.h>
}

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
              int protection_ = PROT_READ|PROT_WRITE);

    ~MemoryMap();

    /// Close the memory map
    /// @return 0 if succeed, -1 else
    int unmap();

    int resize(uint32_t length);

    int get_protection() const {return protection;}
    int get_status() const {return status;}
    uintptr_t get_base_addr() const {return mapped_dev_base;}
    uint32_t mapped_size() const {return size;}
    uintptr_t get_phys_addr() const {return phys_addr;}

    std::tuple<uintptr_t, int, uintptr_t, uint32_t, int>
    get_params() {
        return std::make_tuple(
            mapped_dev_base,
            status,
            phys_addr,
            size,
            protection
        );
    }

    enum Status {
        MEMMAP_CLOSED,       ///< Memory map closed
        MEMMAP_OPENED,       ///< Memory map opened
        MEMMAP_CANNOT_UMMAP, ///< Memory map cannot be unmapped
        MEMMAP_FAILURE       ///< Failure at memory mapping
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

#endif // __DRIVERS_CORE_MEMORY_MAP_HPP__
