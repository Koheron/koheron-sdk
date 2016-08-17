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
    int get_status() {
        return cast_to_memory_map<id>(mem_maps[id])->get_status();
    }

    template<MemMapID id>
    std::tuple<uintptr_t, int, uintptr_t, uint32_t, int>
    get_map_params() {
        return cast_to_memory_map<id>(mem_maps[id])->get_params();
    }

  private:
    kserver::KServer *kserver;
    int fd;         // /dev/mem file ID
    bool is_open;   // True if /dev/mem open

    int add_memory_maps();
    template<MemMapID id> int add_memory_map();

    std::array<std::unique_ptr<MemoryMapBase>, addresses::count> mem_maps;

    // TODO Return int for error status
    template<size_t cnt>
    std::enable_if_t<cnt == 0, void>
    append_maps() {}

    template<size_t cnt>
    std::enable_if_t<(cnt > 0), void>
    append_maps() {
        add_memory_map<cnt-1>();
        append_maps<cnt-1>();
    }

};

#endif // __DRIVERS_LIB_DEV_MEM_HPP__
