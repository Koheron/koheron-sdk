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

#include "memory_map.hpp"

// http://stackoverflow.com/questions/39041236/tuple-of-sequence
template<size_t N, class = std::make_index_sequence<N>> class DevMemImpl;

template<size_t N, MemMapID... ids>
class DevMemImpl<N, std::index_sequence<ids...>>
{
  public:
    DevMemImpl()
    : fd(-1)
    , failed_maps(0)
    {}

    ~DevMemImpl() {close(fd);}

    int open();

    template<MemMapID id>
    MemoryMap<id>& get() {
        return std::get<id>(mem_maps);
    }

  private:
    int fd;
    std::vector<MemMapID> failed_maps;
    std::tuple<MemoryMap<ids>...> mem_maps;

    template<MemMapID id> void open_memory_map();

    template<MemMapID cnt>
    std::enable_if_t<cnt == 0, void>
    create_maps() {}

    template<MemMapID cnt>
    std::enable_if_t<(cnt > 0), void>
    create_maps() {
        open_memory_map<cnt-1>();
        create_maps<cnt-1>();
    }
};

template<size_t N, MemMapID... ids>
int DevMemImpl<N, std::index_sequence<ids...>>::open()
{
    fd = ::open("/dev/mem", O_RDWR | O_SYNC);

     if (fd == -1) {
        fprintf(stderr, "Can't open /dev/mem\n");
        return -1;
    }

    create_maps<N>();

    if (! failed_maps.empty())
        return -1;

    return fd;
}

template<size_t N, MemMapID... ids>
template<MemMapID id>
void DevMemImpl<N, std::index_sequence<ids...>>::open_memory_map()
{
    get<id>().open(fd);

    if (! get<id>().opened()) {
        fprintf(stderr, "Can't open memory map id = %u\n", id);
        failed_maps.push_back(id);
    }
}

using DevMem = DevMemImpl<addresses::count>;

#endif // __DRIVERS_LIB_DEV_MEM_HPP__
