/// Driver memory manager
///
/// (c) Koheron

#ifndef __SERVER_CONTEXT_MEMORY_MANAGER_HPP__
#define __SERVER_CONTEXT_MEMORY_MANAGER_HPP__

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/hardware/memory_map.hpp"

#include <vector>
#include <tuple>

namespace hw {

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

    int open() {
        // Expand over all ids...
        ( [&]{
            const int fd = std::get<ids>(mem_maps).open();
            std::get<ids>(fds) = fd;
            if (fd < 0) {
                logf<ERROR>("MemoryManager: Can't open memory map id = {}\n", ids);
                failed_maps.push_back(ids);
            }
        }(), ... );

        return failed_maps.empty() ? 0 : -1;
    }

    template<MemID id>
    Memory<id>& get() {
        return std::get<id>(mem_maps);
    }

  private:
    std::array<int, N> fds{};
    std::vector<MemID> failed_maps;
    std::tuple<Memory<ids>...> mem_maps;
};

using MemoryManager = MemoryManagerImpl<mem::count>;

// Convinience access to MemoryManager service
template<MemID id>
Memory<id>& get_memory() {
    return services::require<hw::MemoryManager>().get<id>();
}

} // namespace hw

#endif // __SERVER_CONTEXT_MEMORY_MANAGER_HPP__
