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
#include <cassert>
#include <functional>

extern "C" {
    #include <fcntl.h>
    #include <sys/mman.h>
}

namespace kserver {class KServer;}

#include "memory_map.hpp"

class DevMem;

class DevMem
{
  public:
    DevMem(kserver::KServer *kserver_);
    ~DevMem();

    int open();

    template<MemMapID id>
    MemoryMap<id>& get() {
        return std::ref(*cast_to_memory_map<id>(mem_maps[id]));
    }

    uintptr_t get_base_addr(MemMapID id) {
        return addresses::get_base_addr(id);
    }

    template<MemMapID id>
    auto get_status() {
        return cast_to_memory_map<id>(mem_maps[id])->get_status();
    }

    template<MemMapID id>
    auto get_map_params() {
        return cast_to_memory_map<id>(mem_maps[id])->get_params();
    }

  private:
    kserver::KServer *kserver;
    int fd;         // /dev/mem file ID

    int add_memory_maps();
    template<MemMapID id> int add_memory_map();

    std::array<std::unique_ptr<MemoryMapBase>, addresses::count> mem_maps;
    std::vector<MemMapID> failed_maps;

    template<MemMapID cnt>
    std::enable_if_t<cnt == 0, void>
    create_maps() {}

    template<MemMapID cnt>
    std::enable_if_t<(cnt > 0), void>
    create_maps() {
        if (add_memory_map<cnt - 1>() < 0)
            failed_maps.push_back(cnt - 1);

        create_maps<cnt - 1>();
    }

};

#endif // __DRIVERS_LIB_DEV_MEM_HPP__
