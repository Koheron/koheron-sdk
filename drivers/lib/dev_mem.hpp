/// Device memory manager
///
/// (c) Koheron

#ifndef __DRIVERS_LIB_DEV_MEM_HPP__
#define __DRIVERS_LIB_DEV_MEM_HPP__

#include <map>
#include <vector>
#include <array>
#include <cstdint>
#include <string>
#include <memory>
#include <tuple>
#include <assert.h>
#include <functional>

extern "C" {
    #include <fcntl.h>
    #include <sys/mman.h>
}

namespace kserver {class KServer;}

#include "memory_map.hpp"

#include <drivers/addresses.hpp>

typedef uint32_t MemMapID;

class DevMem;

namespace addresses {

constexpr uint32_t count = address_array.size();

// Access elements in address_array

constexpr uintptr_t get_base_addr(MemMapID id) {
    return std::get<0>(address_array[id]);
}

constexpr uint32_t get_range(MemMapID id) {
    return std::get<1>(address_array[id]);
}

constexpr uint32_t get_protection(MemMapID id) {
    return std::get<2>(address_array[id]);
}

} // namespace addresses

template<MemMapID first_id, size_t N>
class MemMapArray
{
  public:
    MemMapArray(DevMem *dvm);

    MemoryMap& operator[](MemMapID id) {
        return maps[id].get();
    }

  private:
    std::vector<std::reference_wrapper<MemoryMap>> maps;
};

class DevMem
{
  public:
    DevMem(kserver::KServer *kserver_);
    ~DevMem();

    int open();

    MemoryMap& get_mmap(MemMapID id) {
        return std::ref(*mem_maps[id].get());
    }

    MemoryMap& operator[](MemMapID id) {
        return get_mmap(id);
    }

    template<MemMapID first_id, size_t N>
    MemMapArray<first_id, N> get_mmaps() {
        return MemMapArray<first_id, N>(this);
    }

    uintptr_t get_base_addr(MemMapID id) {return mem_maps.at(id)->get_base_addr();}
    int get_status(MemMapID id)          {return mem_maps.at(id)->get_status();}

    std::tuple<uintptr_t, int, uintptr_t, uint32_t, int>
    get_map_params(MemMapID id) {
        return mem_maps.at(id)->get_params();
    }

  private:
    kserver::KServer *kserver;
    int fd;         // /dev/mem file ID
    bool is_open;   // True if /dev/mem open

    int add_memory_maps();
    int add_memory_map(MemMapID id);

    std::array<std::unique_ptr<MemoryMap>, addresses::count> mem_maps;
};

template<MemMapID first_id, size_t N>
MemMapArray<first_id, N>::MemMapArray(DevMem *dvm)
{
    for (MemMapID i=0; i<N; i++)
        maps.push_back(dvm->get_mmap(first_id + i));
}

#endif // __DRIVERS_LIB_DEV_MEM_HPP__
