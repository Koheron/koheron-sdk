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

template<size_t N, class = std::make_index_sequence<N>>
class DevMemImpl;

template<size_t N, MemMapID... ids>
class DevMemImpl<N, std::index_sequence<ids...>>
{
  public:
    DevMemImpl(kserver::KServer *kserver_);

    ~DevMemImpl() {close(fd);}

    int open();

    template<MemMapID id>
    MemoryMap<id>& get() {
        return std::get<id>(mem_maps);
    }

  private:
    kserver::KServer *kserver;
    int fd;         // /dev/mem file ID

    template<MemMapID id> void add_memory_map();

    std::vector<MemMapID> failed_maps;

    std::tuple<MemoryMap<ids>...> mem_maps;

    template<MemMapID cnt>
    std::enable_if_t<cnt == 0, void>
    create_maps() {}

    template<MemMapID cnt>
    std::enable_if_t<(cnt > 0), void>
    create_maps() {
        add_memory_map<cnt-1>();
        create_maps<cnt-1>();
    }
};

template<size_t N, MemMapID... ids>
DevMemImpl<N, std::index_sequence<ids...>>::DevMemImpl(kserver::KServer *kserver_)
: kserver(kserver_)
, failed_maps(0)
{
    fd = -1;
}

template<size_t N, MemMapID... ids>
int DevMemImpl<N, std::index_sequence<ids...>>::open()
{
    fd = ::open("/dev/mem", O_RDWR | O_SYNC);

     if (fd == -1) {
        fprintf(stderr, "Can't open /dev/mem\n");
        return -1;
    }

    create_maps<addresses::count>();

    if (! failed_maps.empty())
        return -1;

    return fd;
}

template<size_t N, MemMapID... ids>
template<MemMapID id>
void DevMemImpl<N, std::index_sequence<ids...>>::add_memory_map()
{
    std::get<id>(mem_maps).open(fd);

    if (std::get<id>(mem_maps).get_status() != MemoryMap<id>::MEMMAP_OPENED) {
        fprintf(stderr, "Can't open memory map id = %u\n", id);
        failed_maps.push_back(id);
    }
}

using DevMem = DevMemImpl<addresses::count>;

#endif // __DRIVERS_LIB_DEV_MEM_HPP__
