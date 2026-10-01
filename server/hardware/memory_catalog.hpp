#ifndef __SERVER_CONTEXT_MEMORY_CATALOG_HPP__
#define __SERVER_CONTEXT_MEMORY_CATALOG_HPP__

#include <unistd.h>

#include <memory.hpp>

using MemID = size_t;

namespace mem {
    constexpr uintptr_t get_base_addr(const MemID id) {
        return std::get<0>(memory_array[id]);
    }

    constexpr uint32_t get_range(const MemID id) {
        return std::get<1>(memory_array[id]);
    }

    constexpr uint32_t get_protection(const MemID id) {
        return std::get<2>(memory_array[id]);
    }

    constexpr uint32_t get_n_blocks(const MemID id) {
        return std::get<3>(memory_array[id]);
    }

    constexpr std::string_view get_device_driver(const MemID id) {
        return std::get<4>(memory_array[id]);
    }

    constexpr std::string_view get_name(const MemID id) {
        return std::get<5>(memory_array[id]);
    }

    constexpr bool is_writable(const MemID id) {
        return (get_protection(id) & PROT_WRITE) == PROT_WRITE;
    }

    constexpr bool is_readable(const MemID id) {
        return (get_protection(id) & PROT_READ) == PROT_READ;
    }

    constexpr uint32_t get_total_size(const MemID id) {
        return get_range(id) * get_n_blocks(id);
    }
} // namespace mem

inline std::size_t page_size() noexcept {
    static std::size_t sz = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
    return sz;
}

inline std::size_t align_up(std::size_t x, std::size_t a) noexcept {
    return (x + a - 1) & ~(a - 1);
}

#endif // __SERVER_CONTEXT_MEMORY_CATALOG_HPP__
