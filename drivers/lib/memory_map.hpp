/// Memory map devices
///
/// (c) Koheron

#ifndef __DRIVERS_LIB_MEMORY_MAP_HPP__
#define __DRIVERS_LIB_MEMORY_MAP_HPP__

#include <cstdio>
#include <cstdint>
#include <tuple>
#include <memory>

extern "C" {
    #include <unistd.h>
    #include <sys/mman.h>
}

#include <drivers/addresses.hpp>

typedef size_t MemMapID;

namespace addresses {

constexpr uint32_t count = address_array.size();

// Access elements in address_array

constexpr uintptr_t get_base_addr(const MemMapID id) {
    return std::get<0>(address_array[id]);
}

// Makes sure it gets evaluated at compile time
static_assert(get_base_addr(CONFIG_MEM) == std::get<0>(address_array[CONFIG_MEM]), "get_base_address test failed");

constexpr uint32_t get_range(const MemMapID id) {
    return std::get<1>(address_array[id]);
}

constexpr uint32_t get_protection(const MemMapID id) {
    return std::get<2>(address_array[id]);
}

constexpr uint32_t get_n_blocks(const MemMapID id) {
    return std::get<3>(address_array[id]);
}

constexpr bool is_writable(const MemMapID id) {
    return (get_protection(id) & PROT_WRITE) == PROT_WRITE;
}

constexpr bool is_readable(const MemMapID id) {
    return (get_protection(id) & PROT_READ) == PROT_READ;
}

constexpr uint32_t get_total_size(const MemMapID id) {
    return get_range(id) * get_n_blocks(id);
}

} // namespace addresses

static constexpr off_t get_mmap_offset(uintptr_t phys_addr, uint32_t size) {
    return phys_addr & ~(size - 1);
}

template<MemMapID id,
         uintptr_t phys_addr = addresses::get_base_addr(id),
         uint32_t n_blocks = addresses::get_n_blocks(id),
         uint32_t block_size = addresses::get_range(id),
         uint32_t size = addresses::get_total_size(id),
         int protection = addresses::get_protection(id)>
class MemoryMap
{
  public:
    static_assert(id < addresses::count, "Invalid ID");

    MemoryMap()
    : mapped_base(nullptr)
    , base_address(0)
    , status(MEMMAP_CLOSED)
    {}

    ~MemoryMap() {
        if (status == MEMMAP_OPENED)
            munmap(mapped_base, size);
    }

    void open(const int& fd) {
        mapped_base = mmap(0, size, protection, MAP_SHARED, fd, get_mmap_offset(phys_addr, size));

        if (mapped_base == (void *) -1) {
            status = MEMMAP_FAILURE;
            return;
        }

        status = MEMMAP_OPENED;
        base_address = reinterpret_cast<uintptr_t>(mapped_base) + (phys_addr & (size - 1));
        // printf("base_address = %u\n", base_address);
    }

    int get_protection() const {return protection;}
    int get_status() const {return status;}
    uintptr_t get_base_addr() const {return base_address;}
    uint32_t mapped_size() const {return size;}
    uintptr_t get_phys_addr() const {return phys_addr;}

    auto get_params() {
        return std::make_tuple(
            base_address,
            status,
            phys_addr,
            size,
            protection
        );
    }

    ////////////////////////////////////////
    // Write functions
    ////////////////////////////////////////

    // Write a register (offset defined at compile-time)
    template<uint32_t offset, typename T = uint32_t>
    void write(T value) {
        static_assert(offset < addresses::get_range(id), "Invalid offset");
        static_assert(addresses::is_writable(id), "Not writable");

        *(volatile T *) (base_address + offset) = value;
    }

    // Write a register (offset defined at run-time)
    template<typename T = uint32_t>
    void write_offset(uint32_t offset, T value) {
        static_assert(addresses::is_writable(id), "Not writable");
        *(volatile T *) (base_address + offset) = value;
    }

    template<typename T = uint32_t, uint32_t offset = 0>
    void set_ptr(const T *data_ptr, uint32_t buff_size, uint32_t block_idx = 0) {
        static_assert(offset < addresses::get_range(id), "Invalid offset");
        static_assert(addresses::is_writable(id), "Not writable");

        uintptr_t addr = base_address + block_size * block_idx + offset;
        for (uint32_t i=0; i < buff_size; i++)
            *(volatile T *) (addr + sizeof(T) * i) = data_ptr[i];
    }

    template<typename T = uint32_t>
    void set_ptr_offset(uint32_t offset, const T *data_ptr, uint32_t buff_size) {
        static_assert(addresses::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        for (uint32_t i=0; i < buff_size; i++)
            *(volatile T *) (addr + sizeof(T) * i) = data_ptr[i];
    }

    // Write a std::array (offset defined at compile-time)
    template<typename T, size_t N, uint32_t offset = 0>
    void write_array(const std::array<T, N> arr) {
        static_assert(offset + sizeof(T) * (N - 1) < addresses::get_range(id), "Invalid offset");
        static_assert(addresses::is_writable(id), "Not writable");

        set_ptr<T, offset>(arr.data(), N);
    }

    // Write a std::array (offset defined at run-time)
    template<typename T, size_t N>
    void write_array_offset(uint32_t offset, const std::array<T, N> arr) {
        static_assert(addresses::is_writable(id), "Not writable");
        set_ptr_offset<T>(offset, arr.data(), N);
    }

    template<uint32_t offset, uint32_t mask, typename T = uint32_t>
    void write_mask(uint32_t value) {
        static_assert(offset < addresses::get_range(id), "Invalid offset");
        static_assert(addresses::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = (*((volatile uintptr_t *) addr) & ~mask) | (value & mask);
    }

    void write_mask_offset(uint32_t offset, uint32_t mask, uint32_t value) {
        static_assert(addresses::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = (*((volatile uintptr_t *) addr) & ~mask) | (value & mask);
    }

    ////////////////////////////////////////
    // Read functions
    ////////////////////////////////////////

    // Read a register (offset defined at compile-time)
    template<uint32_t offset, typename T = uint32_t>
    T read() {
        static_assert(offset < addresses::get_range(id), "Invalid offset");
        static_assert(addresses::is_readable(id), "Not readable");

        return *(volatile T *) (base_address + offset);
    }

    // Read a register (offset defined at run-time)
    template<typename T = uint32_t>
    T read_offset(uint32_t offset) {
        static_assert(addresses::is_readable(id), "Not readable");
        return *(volatile T *) (base_address + offset);
    }

    template<typename T = uint32_t, uint32_t offset = 0>
    T* get_ptr(uint32_t block_idx = 0) {
        static_assert(offset < addresses::get_range(id), "Invalid offset");
        static_assert(addresses::is_readable(id), "Not readable");

        return reinterpret_cast<T*>(base_address + block_size * block_idx + offset);
    }

    template<typename T = uint32_t>
    T* get_ptr_offset(uint32_t offset = 0) {
        static_assert(addresses::is_readable(id), "Not readable");
        return reinterpret_cast<T*>(base_address + offset);
    }

    // Read a std::array (offset defined at compile-time)
    template<typename T, size_t N, uint32_t offset = 0>
    std::array<T, N>& read_array(uint32_t block_idx = 0) {
        static_assert(offset + sizeof(T) * (N - 1) < addresses::get_range(id), "Invalid offset");
        static_assert(addresses::is_readable(id), "Not readable");

        auto p = get_ptr<std::array<T, N>, offset>(block_idx);
        return *p;
    }

    // Read a std::array (offset defined at run-time)
    template<typename T, size_t N>
    std::array<T, N>& read_array_offset(uint32_t offset) {
        static_assert(addresses::is_readable(id), "Not readable");

        auto p = get_ptr_offset<std::array<T, N>>(offset);
        return *p;
    }

    ////////////////////////////////////////
    // Bit manipulation
    ////////////////////////////////////////

    // Set a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    void set_bit() {
        static_assert(offset < addresses::get_range(id), "Invalid offset");
        static_assert(addresses::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) | (1 << index);
    }

    // Set a bit (offset and index defined at run-time)
    void set_bit_offset(uint32_t offset, uint32_t index) {
        static_assert(addresses::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) | (1 << index);
    }

    // Clear a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    void clear_bit() {
        static_assert(offset < addresses::get_range(id), "Invalid offset");
        static_assert(addresses::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) & ~(1 << index);
    }

    // Clear a bit (offset and index defined at run-time)
    void clear_bit_offset(uint32_t offset, uint32_t index) {
        static_assert(addresses::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) & ~(1 << index);
    }

    // Toggle a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    void toggle_bit() {
        static_assert(offset < addresses::get_range(id), "Invalid offset");
        static_assert(addresses::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) ^ (1 << index);
    }

    // Toggle a bit (offset and index defined at run-time)
    void toggle_bit_offset(uint32_t offset, uint32_t index) {
        static_assert(addresses::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) ^ (1 << index);
    }

    // Read a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    bool read_bit() {
        static_assert(offset < addresses::get_range(id), "Invalid offset");
        static_assert(addresses::is_readable(id), "Not readable");

        return *((volatile uint32_t *) (base_address + offset)) & (1 << index);
    }

    // Read a bit (offset and index defined at run-time)
    bool read_bit_offset(uint32_t offset, uint32_t index) {
        static_assert(addresses::is_readable(id), "Not readable");
        return *((volatile uint32_t *) (base_address + offset)) & (1 << index);
    }

    enum Status {
        MEMMAP_CLOSED,       ///< Memory map closed
        MEMMAP_OPENED,       ///< Memory map opened
        MEMMAP_FAILURE       ///< Failure at memory mapping
    };

  private:
    void *mapped_base;       ///< Map base address
    uintptr_t base_address;  ///< Virtual memory base address of the device
    int status;              ///< Status
};

#endif // __DRIVERS_CORE_MEMORY_MAP_HPP__
