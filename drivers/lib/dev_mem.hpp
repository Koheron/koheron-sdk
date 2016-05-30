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
#include <assert.h> 

extern "C" {
    #include <fcntl.h>
}

#include "memory_map.hpp"
#include "wr_register.hpp"

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

    // Helper function to check the IDs returned by RequestMemoryMaps
    template<size_t N>
    int CheckMapIDs(std::array<MemMapID, N> ids);

    inline int Resize(MemMapID id, uint32_t length)
    {
        assert(mem_maps.at(id) != nullptr);
        return mem_maps.at(id)->Resize(length);
    }

    /// Create a new memory map
    /// @addr Base address of the map
    /// @size Size of the map 
    /// @return An ID to the created map,
    ///         or -1 if an error occured
    MemMapID AddMemoryMap(uintptr_t addr, uint32_t size);
    
    /// Remove a memory map
    /// @id ID of the memory map to be removed
    void RmMemoryMap(MemMapID id);
    
    /// Remove all the memory maps
    void RemoveAll();
    
    /// Get a memory map
    /// @id ID of the memory map
    inline MemoryMap& GetMemMap(MemMapID id)
    {
        assert(mem_maps.at(id) != nullptr);
        return *mem_maps.at(id);
    }
    
    /// Return the base address of a map
    /// @id ID of the map
    inline uintptr_t GetBaseAddr(MemMapID id)
    {
        assert(mem_maps.at(id) != nullptr);
        return mem_maps.at(id)->GetBaseAddr();
    }
    
    /// Return the status of a map
    /// @id ID of the map
    inline int GetStatus(MemMapID id)
    {
        assert(mem_maps.at(id) != nullptr);   
        return mem_maps.at(id)->GetStatus();
    }
	
    /// Return 1 if a memory map failed
    int IsFailed();

    inline void write32(MemMapID id, uint32_t offset, uint32_t value)
    {
        WriteReg32(GetBaseAddr(id) + offset, value);
    }

    inline uint32_t read32(MemMapID id, uint32_t offset)
    {
        return ReadReg32(GetBaseAddr(id) + offset);
    }

    inline uint32_t read32(MemMapID id, uint32_t offset)
    {
        return ReadReg32(GetBaseAddr(id) + offset);
    }

    inline uint32_t set_bit(MemMapID id, uint32_t offset, uint32_t index)
    {
        return SetBit(GetBaseAddr(id) + offset, index);
    }

    inline uint32_t clear_bit(MemMapID id, uint32_t offset, uint32_t index)
    {
        return ClearBit(GetBaseAddr(id) + offset, index);
    }
    
    /// True if the /dev/mem device is open
    inline bool IsOpen() const {return is_open;}

  private:
    int fd;         ///< /dev/mem file ID
    bool is_open;   ///< True if /dev/mem open
    
    /// Limit addresses
    uintptr_t addr_limit_down;
    uintptr_t addr_limit_up;
    bool __is_forbidden_address(uintptr_t addr);

    std::map<MemMapID, MemoryMap*> mem_maps;
    MemMapIdPool id_pool;
};

// Helper to build an std::array of memmory regions without
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
        bool region_is_mapped = false;

        for (auto& mem_map : mem_maps) {
            if (region.phys_addr == mem_map.second->PhysAddr()) {
                // we resize the map if the new range is large
                // than the previously allocated one.
                if (region.range > mem_map.second->MappedSize())
                    if (Resize(mem_map.first, region.range) < 0) {
                        fprintf(stderr, "Memory map resizing failed\n");
                        region_is_mapped = true;
                        break;
                    }

                map_ids[i] = mem_map.first;
                region_is_mapped = true;
                break;
            }
        }

        if (!region_is_mapped)
            map_ids[i] = AddMemoryMap(region.phys_addr, region.range);

        i++;
    }

    return map_ids;
}

template<size_t N>
int DevMem::CheckMapIDs(std::array<MemMapID, N> ids)
{
    for (auto& id : ids)
        if (static_cast<int>(id) < 0)
            return -1;

    return 0;
}

}; // namespace Klib

#endif // __DRIVERS_CORE_DEV_MEM_HPP__
