/// (c) Koheron

#include "dev_mem.hpp"

/// @namespace Klib
/// @brief Namespace of the Koheron library
namespace Klib {

MemMapID MemMapIdPool::get_id(unsigned int num_maps)
{
    MemMapID new_id;
    
    // Choose a reusable ID if available else
    // create a new ID equal to the number of memmaps
    if (reusable_ids.size() == 0) {    
        new_id = static_cast<MemMapID>(num_maps);
    } else {
        new_id = reusable_ids.back();
        reusable_ids.pop_back();
    }

    return new_id;
}

void MemMapIdPool::release_id(MemMapID id)
{
    reusable_ids.push_back(id);
}

DevMem::DevMem(uintptr_t addr_limit_down_, uintptr_t addr_limit_up_)
: addr_limit_down(addr_limit_down_),
  addr_limit_up(addr_limit_up_),
  mem_maps()
{
    fd = -1;
    is_open = 0;
}

DevMem::~DevMem()
{
    Close();
}

int DevMem::Open()
{
    // Open /dev/mem if not already open
    if (fd == -1) {
        fd = open("/dev/mem", O_RDWR | O_SYNC);

         if (fd == -1) {
            fprintf(stderr, "Can't open /dev/mem\n");
            return -1;
        }
        
        is_open = 1;
    }

    RemoveAll(); // Reset memory maps
    return fd;
}

int DevMem::Close()
{
    RemoveAll();
    close(fd);
    return 0;
}

unsigned int DevMem::num_maps = 0;

bool DevMem::is_forbidden_address(uintptr_t addr)
{
    if (addr_limit_up == 0x0 && addr_limit_down == 0x0)
        return false; // No limit defined
    else
        return (addr > addr_limit_up) || (addr < addr_limit_down);
}

MemMapID DevMem::create_memory_map(uintptr_t addr, uint32_t size, int protection)
{
    if (is_forbidden_address(addr)) {
        fprintf(stderr,"Forbidden memory region\n");
        return static_cast<MemMapID>(-1);
    }

    auto mem_map = std::make_unique<MemoryMap>(&fd, addr, size, protection);
    assert(mem_map != nullptr);

    if (mem_map->get_status() != MemoryMap::MEMMAP_OPENED) {
        fprintf(stderr,"Can't open memory map\n");
        return static_cast<MemMapID>(-1);
    }

    MemMapID new_id = id_pool.get_id(num_maps);
    mem_maps.insert(std::pair<MemMapID, std::unique_ptr<MemoryMap>>(new_id, std::move(mem_map)));
    num_maps++;
    return new_id;
}

MemMapID DevMem::AddMemoryMap(uintptr_t addr, uint32_t size, int protection)
{
    bool region_is_mapped = false;
    MemMapID map_id = static_cast<MemMapID>(-1);

    for (auto& mem_map : mem_maps) {
        if (addr == mem_map.second->get_phys_addr()) {
            // we resize the map if the new range is large
            // than the previously allocated one.
            if (size > mem_map.second->mapped_size())
                if (resize(mem_map.first, size) < 0) {
                    fprintf(stderr, "Memory map resizing failed\n");
                    region_is_mapped = true;
                    break;
                }

            map_id = mem_map.first;
            region_is_mapped = true;
            break;
        }
    }

    if (!region_is_mapped)
        map_id = create_memory_map(addr, size, protection);

    return map_id;
}

void DevMem::RmMemoryMap(MemMapID id)
{
    mem_maps.erase(id);
    id_pool.release_id(id);
    num_maps--;
}

void DevMem::RemoveAll()
{
    assert(num_maps == mem_maps.size());

    if (!mem_maps.empty()) {
        // This is necessary because both mem_maps.size() and num_maps
        // are decremented at each call of RmMemoryMap
        uint32_t mem_maps_size = mem_maps.size();
    
        for (unsigned int id=0; id<mem_maps_size; id++)
            RmMemoryMap(id);
    }

    assert(num_maps == 0);
}

int DevMem::IsFailed()
{
    for (unsigned int i=0; i<mem_maps.size(); i++)
        if (mem_maps[i]->get_status() == MemoryMap::MEMMAP_FAILURE)
            return 1;
    return 0;
}

}; // namespace Klib

