/// Driver memory manager
///
/// (c) Koheron

#ifndef __DRIVERS_LIB_MEMORY_MANAGER_HPP__
#define __DRIVERS_LIB_MEMORY_MANAGER_HPP__

#include "server/runtime/syslog.hpp"
#include "server/context/memory_map.hpp"

#include <vector>
#include <tuple>

// http://stackoverflow.com/questions/39041236/tuple-of-sequence
template<size_t N, class = std::make_index_sequence<N>> class MemoryManagerImpl;

template<size_t N, MemID... ids>
class MemoryManagerImpl<N, std::index_sequence<ids...>>
{
  public:
    MemoryManagerImpl()
    : failed_maps(0)
    {}

    ~MemoryManagerImpl() {
        for (int& fd : fds) {
            if (fd >= 0) {
                ::close(fd);
            }
        }
    }

    int open();

    template<MemID id>
    Memory<id>& get() {
        return std::get<id>(mem_maps);
    }

  private:
    std::array<int, N> fds{};
    std::vector<MemID> failed_maps;
    std::tuple<Memory<ids>...> mem_maps;

    template<MemID id> void open_memory_map();

    template<MemID cnt>
    void open_maps() {
        if constexpr (cnt > 0) {
            open_memory_map<cnt-1>();
            open_maps<cnt-1>();
        }
    }
};

template<size_t N, MemID... ids>
int MemoryManagerImpl<N, std::index_sequence<ids...>>::open() {
    open_maps<N>();

    if (!failed_maps.empty()) {
        return -1;
    }

    return 0;
}

template<size_t N, MemID... ids>
template<MemID id>
void MemoryManagerImpl<N, std::index_sequence<ids...>>::open_memory_map() {
    const int fd = get<id>().open();
    std::get<id>(fds) = fd;

    if (fd< 0) {
        koheron::print_fmt<ERROR>("MemoryManager: Can't open memory map id = {}\n", id);
        failed_maps.push_back(id);
    }
}

using MemoryManager = MemoryManagerImpl<mem::count>;

#endif // __DRIVERS_LIB_MEMORY_MANAGER_HPP__