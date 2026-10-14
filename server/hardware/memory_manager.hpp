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
    using OpenResult = std::expected<void, std::vector<MemoryOpenError>>;

    MemoryManagerImpl() = default;

    int open() {
        const auto result = open_checked();
        if (result) { return 0; }
        for (const auto& error : result.error()) { report_memory_open_error(error); }
        errno = memory_error_errno(result.error().front());
        return -1;
    }

    [[nodiscard]] OpenResult open_checked() {
        std::vector<MemoryOpenError> failures;
        // Expand over all ids...
        ( [&]{
            if (const auto result = std::get<ids>(mem_maps).open_checked(); !result) {
                failures.push_back(result.error());
            }
        }(), ... );

        if (!failures.empty()) { return std::unexpected(std::move(failures)); }
        return {};
    }

    template<MemID id>
    Memory<id>& get() {
        return std::get<id>(mem_maps);
    }

  private:
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
