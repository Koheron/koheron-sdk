/// Device memory manager
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_DEV_MEM_HPP__
#define __DRIVERS_CORE_DEV_MEM_HPP__

#include <map>
#include <vector>
#include <array>
#include <cstdint>
#include <string>
#include <memory>
#include <assert.h> 

extern "C" {
    #include <fcntl.h>
    #include <sys/mman.h>
}

#include "memory_map.hpp"

/// @namespace Klib
/// @brief Namespace of the Koheron library
namespace Klib {

struct MemoryRegion
{
    uintptr_t phys_addr;
    uint32_t range;
};

/// ID of a memory map
typedef uint32_t MemMapID;

class MemMapIdPool
{
  public:
    MemMapIdPool() : reusable_ids(0) {};

    MemMapID get_id(unsigned int num_maps);
    void release_id(MemMapID id);
  private:
    std::vector<MemMapID> reusable_ids;
};

#define ASSERT_WRITABLE assert((mem_maps.at(id)->GetProtection() & PROT_WRITE) == PROT_WRITE);

#define ASSERT_READABLE                                                     \
    assert(   mem_maps.at(id)->GetProtection() == PROT_READ                 \
           || mem_maps.at(id)->GetProtection() == (PROT_READ|PROT_WRITE));

/// Device memory manager
/// A memory maps factory
class DevMem
{
  public:
    DevMem(uintptr_t addr_limit_down_=0x0, uintptr_t addr_limit_up_=0x0);
    ~DevMem();

    /// Open the /dev/mem driver
    int Open();

    /// Close all the memory maps
    /// @return 0 if succeed, -1 else
    int Close();

    /// Current number of memory maps
    static unsigned int num_maps;

    template<size_t N>
    std::array<MemMapID, N> 
    RequestMemoryMaps(std::array<MemoryRegion, N> regions);

    int CheckMap(MemMapID id) {return static_cast<int>(id);}

    template<typename... map_id> int CheckMaps(map_id... id);

    // Helper function to check the IDs returned by RequestMemoryMaps
    template<size_t N>
    int CheckMapIDs(std::array<MemMapID, N> ids);

    int Resize(MemMapID id, uint32_t length) {return mem_maps.at(id)->Resize(length);}

    /// Create a new memory map
    /// @addr Base address of the map
    /// @size Size of the map
    /// @permissions Access permissions
    /// @return An ID to the created map,
    ///         or -1 if an error occured
    MemMapID AddMemoryMap(uintptr_t addr, uint32_t size, 
                          int permissions = PROT_READ|PROT_WRITE);

    /// Remove a memory map
    /// @id ID of the memory map to be removed
    void RmMemoryMap(MemMapID id);

    /// Remove all the memory maps
    void RemoveAll();

    uintptr_t GetBaseAddr(MemMapID id) {return mem_maps.at(id)->GetBaseAddr();}
    int GetStatus(MemMapID id)         {return mem_maps.at(id)->GetStatus();}

    /// Return 1 if a memory map failed
    int IsFailed();

    bool is_ok() {return !IsFailed();}

    void write32(MemMapID id, uint32_t offset, uint32_t value) {
        ASSERT_WRITABLE
        *(volatile uintptr_t *) (GetBaseAddr(id) + offset) = value;
    }

    void write_buff32(MemMapID id, uint32_t offset, 
                      const uint32_t *data_ptr, uint32_t buff_size) {
        ASSERT_WRITABLE
        uintptr_t addr = GetBaseAddr(id) + offset;
        for(uint32_t i=0; i < buff_size; i++)
            *(volatile uintptr_t *) (addr + sizeof(uint32_t) * i) = data_ptr[i];
    }

    uint32_t read32(MemMapID id, uint32_t offset) {
        ASSERT_READABLE
        return *(volatile uintptr_t *) (GetBaseAddr(id) + offset);
    }

    void set_bit(MemMapID id, uint32_t offset, uint32_t index) {
        ASSERT_WRITABLE
        uintptr_t addr = GetBaseAddr(id) + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) | (1 << index);
    }

    void clear_bit(MemMapID id, uint32_t offset, uint32_t index) {
        ASSERT_WRITABLE
        uintptr_t addr = GetBaseAddr(id) + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) & ~(1 << index);
    }

    void toggle_bit(MemMapID id, uint32_t offset, uint32_t index) {
        ASSERT_WRITABLE
        uintptr_t addr = GetBaseAddr(id) + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) ^ (1 << index);
    }

    bool read_bit(MemMapID id, uint32_t offset, uint32_t index) {
        ASSERT_READABLE
        return *((volatile uintptr_t *) (GetBaseAddr(id) + offset)) & (1 << index);
    }

    void mask_and(MemMapID id, uint32_t offset, uint32_t mask) {
        ASSERT_WRITABLE
        uintptr_t addr = GetBaseAddr(id) + offset;
        *(volatile uintptr_t *) addr &= mask;
    }

    void mask_or(MemMapID id, uint32_t offset, uint32_t mask) {
        ASSERT_WRITABLE
        uintptr_t addr = GetBaseAddr(id) + offset;
        *(volatile uintptr_t *) addr |= mask;
    }

    /// True if the /dev/mem device is open
    bool IsOpen() const {return is_open;}

  private:
    int fd;         ///< /dev/mem file ID
    bool is_open;   ///< True if /dev/mem open
    
    /// Limit addresses
    uintptr_t addr_limit_down;
    uintptr_t addr_limit_up;
    bool is_forbidden_address(uintptr_t addr);
    MemMapID create_memory_map(uintptr_t addr, uint32_t size, int permissions);

    std::map<MemMapID, std::unique_ptr<MemoryMap>> mem_maps;
    MemMapIdPool id_pool;
};


// Helper to build an std::array of memory regions without
// specifying the length. Called as:
// mem_regions(
//     Klib::MemoryRegion({ ADDR1, RANGE1 }),
//     ...
//     Klib::MemoryRegion({ ADDRn, RANGEn })
// )
template<typename... region>
constexpr auto mem_regions(region&&... args) 
    -> std::array<Klib::MemoryRegion, sizeof...(args)>
{
    return {{std::forward<region>(args)...}};
}

template<size_t N>
std::array<MemMapID, N> 
DevMem::RequestMemoryMaps(std::array<MemoryRegion, N> regions)
{
    auto map_ids = std::array<MemMapID, N>();
    map_ids.fill(static_cast<MemMapID>(-1));
    uint32_t i = 0;

    for (auto& region : regions) {
        map_ids[i] = AddMemoryMap(region.phys_addr, region.range);
        i++;
    }

    return map_ids;
}

template<size_t N>
int DevMem::CheckMapIDs(std::array<MemMapID, N> ids)
{
    for (auto& id : ids)
        if (CheckMap(id) < 0)
            return -1;

    return 0;
}

template<typename... map_id>
constexpr auto ids_array(map_id&&... args) 
    -> std::array<MemMapID, sizeof...(args)>
{
    return {{std::forward<map_id>(args)...}};
}

template<typename... map_id>
int DevMem::CheckMaps(map_id... id)
{
    return CheckMapIDs(ids_array(id...));
}

}; // namespace Klib

#endif // __DRIVERS_CORE_DEV_MEM_HPP__
