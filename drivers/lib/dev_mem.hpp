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

    int add_memory_maps(const std::array<std::tuple<uintptr_t, uint32_t, int>, NUM_ADDRESSES>& addresses);

    MemoryMap& get_mmap(MemMapID id) {
        return std::ref(*mem_maps[id].get());
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

    int add_memory_map(MemMapID id, const std::tuple<uintptr_t, uint32_t, int>& addr);

    std::array<std::unique_ptr<MemoryMap>, NUM_ADDRESSES> mem_maps;
};

template<MemMapID first_id, size_t N>
MemMapArray<first_id, N>::MemMapArray(DevMem *dvm)
{
    for (MemMapID i=0; i<N; i++)
        maps.push_back(dvm->get_mmap(first_id + i));
}

#endif // __DRIVERS_LIB_DEV_MEM_HPP__
