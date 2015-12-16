/// Device memory manager
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_DEV_MEM_HPP__
#define __DRIVERS_CORE_DEV_MEM_HPP__

#include <map>
#include <vector>
#include <cstdint>
#include <assert.h> 

extern "C" {
    #include <fcntl.h>
}

#include "memory_map.hpp"

/// @namespace Klib
/// @brief Namespace of the Koheron library
namespace Klib {

/// ID of a memory map
typedef uint32_t MemMapID;

/// Device memory manager
/// A memory maps factory
class DevMem
{
  public:
    DevMem(intptr_t addr_limit_down_=0x0, intptr_t addr_limit_up_=0x0);
    ~DevMem();

    /// Open the /dev/mem driver
    int Open();
	
    /// Close all the memory maps
    /// @return 0 if succeed, -1 else
    int Close();

    /// Current number of memory maps
    static unsigned int num_maps;

    /// Create a new memory map
    /// @addr Base address of the map
    /// @size Size of the map 
    /// @return An ID to the created map,
    ///         or -1 if an error occured
    MemMapID AddMemoryMap(intptr_t addr, uint32_t size);
    
    /// Remove a memory map
    /// @id ID of the memory map to be removed
    void RmMemoryMap(MemMapID id);
    
    /// Remove all the memory maps
    void RemoveAll();
    
    /// Get a memory map
    /// @id ID of the memory map
    MemoryMap& GetMemMap(MemMapID id);
    
    /// Return the base address of a map
    /// @id ID of the map
    uint32_t GetBaseAddr(MemMapID id);
    
    /// Return the status of a map
    /// @id ID of the map
    int GetStatus(MemMapID id);
	
    /// Return 1 if a memory map failed
    int IsFailed();
    
    /// True if the /dev/mem device is open
    inline bool IsOpen() const {return is_open;}

  private:
    int fd;         ///< /dev/mem file ID
    bool is_open;   ///< True if /dev/mem open
    
    /// Limit addresses
    intptr_t addr_limit_down;
    intptr_t addr_limit_up;
    bool __is_forbidden_address(intptr_t addr);
    
    /// Memory maps container
    std::map<MemMapID, MemoryMap*> mem_maps; 
    std::vector<MemMapID> reusable_ids;
};

}; // namespace Klib

#endif // __DRIVERS_CORE_DEV_MEM_HPP__
