/// Driver memory manager
///
/// (c) Koheron

#ifndef __DRIVERS_LIB_MEMORY_MANAGER_HPP__
#define __DRIVERS_LIB_MEMORY_MANAGER_HPP__

#include "server/runtime/syslog.hpp"
#include "server/context/memory_map.hpp"

#include <vector>
#include <tuple>
#include <fcntl.h>
#include <sys/mman.h>

// http://stackoverflow.com/questions/39041236/tuple-of-sequence
template<size_t N, class = std::make_index_sequence<N>> class MemoryManagerImpl;

template<size_t N, MemID... ids>
class MemoryManagerImpl<N, std::index_sequence<ids...>>
{
  public:
    MemoryManagerImpl()
    : fd(-1)
    , failed_maps(0)
    {}

    ~MemoryManagerImpl() {close(fd);}

    int open();

    template<MemID id>
    Memory<id>& get() {
        return std::get<id>(mem_maps);
    }

  private:
    int fd;
    std::vector<MemID> failed_maps;
    std::tuple<Memory<ids>...> mem_maps;

    template<MemID id> void open_memory_map();

    template<MemID cnt>
    std::enable_if_t<cnt == 0, void>
    open_maps() {}

    template<MemID cnt>
    std::enable_if_t<(cnt > 0), void>
    open_maps() {
        open_memory_map<cnt-1>();
        open_maps<cnt-1>();
    }
};

template<size_t N, MemID... ids>
int MemoryManagerImpl<N, std::index_sequence<ids...>>::open()
{
    fd = ::open("/dev/mem", O_RDWR | O_SYNC);

     if (fd == -1) {
        koheron::print<ERROR>("Can't open /dev/mem\n");
        return -1;
    }

    open_maps<N>();

    if (!failed_maps.empty()) {
        return -1;
    }

    return fd;
}

template<size_t N, MemID... ids>
template<MemID id>
void MemoryManagerImpl<N, std::index_sequence<ids...>>::open_memory_map()
{
    get<id>().open(fd);

    if (!get<id>().is_open()) {
        koheron::print_fmt<ERROR>("Can't open memory map id = {}\n", id);
        failed_maps.push_back(id);
    }
}

using MemoryManager = MemoryManagerImpl<mem::count>;

#endif // __DRIVERS_LIB_MEMORY_MANAGER_HPP__
