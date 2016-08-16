/// Memory map devices
///
/// (c) Koheron

#ifndef __DRIVERS_LIB_MEMORY_MAP_HPP__
#define __DRIVERS_LIB_MEMORY_MAP_HPP__

#include <cstdio>
#include <cstdint>
#include <tuple>

extern "C" {
    #include <unistd.h>
    #include <sys/mman.h>
}

#define DEFAULT_MAP_SIZE 4096UL // = PAGE_SIZE
#define MAP_MASK(size) ((size) - 1)

// #define ASSERT_WRITABLE assert((protection & PROT_WRITE) == PROT_WRITE);
// #define ASSERT_READABLE assert((protection & PROT_READ) == PROT_READ);
#define ASSERT_WRITABLE
#define ASSERT_READABLE

class MemoryMap
{
  public:
    /// Build a memory map
    /// @phys_addr_ Physical base address of the device
    /// @size_ Map size in octets
    MemoryMap(int *fd_, uintptr_t phys_addr_,
              uint32_t size_ = DEFAULT_MAP_SIZE,
              int protection_ = PROT_READ|PROT_WRITE);

    ~MemoryMap();

    /// Close the memory map
    /// @return 0 if succeed, -1 else
    int unmap();

    int resize(uint32_t length);

    int get_protection() const {return protection;}
    int get_status() const {return status;}
    uintptr_t get_base_addr() const {return base_address;}
    uint32_t mapped_size() const {return size;}
    uintptr_t get_phys_addr() const {return phys_addr;}

    std::tuple<uintptr_t, int, uintptr_t, uint32_t, int>
    get_params() {
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
        ASSERT_WRITABLE
        *(volatile T *) (base_address + offset) = value;
    }

    // Write a register (offset defined at run-time)
    template<typename T = uint32_t>
    void write_offset(uint32_t offset, T value) {
        ASSERT_WRITABLE
        *(volatile T *) (base_address + offset) = value;
    }

    template<typename T = uint32_t, uint32_t offset = 0>
    void set_ptr(const T *data_ptr, uint32_t buff_size) {
        ASSERT_WRITABLE
        uintptr_t addr = base_address + offset;
        for (uint32_t i=0; i < buff_size; i++)
            *(volatile T *) (addr + sizeof(T) * i) = data_ptr[i];
    }

    template<typename T = uint32_t>
    void set_ptr_offset(uint32_t offset, const T *data_ptr, uint32_t buff_size) {
        ASSERT_WRITABLE
        uintptr_t addr = base_address + offset;
        for (uint32_t i=0; i < buff_size; i++)
            *(volatile T *) (addr + sizeof(T) * i) = data_ptr[i];
    }

    // Write a std::array (offset defined at compile-time)
    template<typename T, size_t N, uint32_t offset = 0>
    void write_array(const std::array<T, N> arr) {
        set_ptr<T, offset>(arr.data(), N);
    }

    // Write a std::array (offset defined at run-time)
    template<typename T, size_t N>
    void write_array_offset(uint32_t offset, const std::array<T, N> arr) {
        set_ptr_offset<T>(offset, arr.data(), N);
    }

    template<uint32_t offset, uint32_t mask, typename T = uint32_t>
    void write_mask(uint32_t value) {
        ASSERT_WRITABLE
        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = (*((volatile uintptr_t *) addr) & ~mask) | (value & mask);
    }

    void write_mask_offset(uint32_t offset, uint32_t mask, uint32_t value) {
        ASSERT_WRITABLE
        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = (*((volatile uintptr_t *) addr) & ~mask) | (value & mask);
    }

    ////////////////////////////////////////
    // Read functions
    ////////////////////////////////////////

    // Read a register (offset defined at compile-time)
    template<uint32_t offset, typename T = uint32_t>
    T read() {
        ASSERT_READABLE
        return *(volatile T *) (base_address + offset);
    }

    // Read a register (offset defined at run-time)
    template<typename T = uint32_t>
    T read_offset(uint32_t offset) {
        ASSERT_READABLE
        return *(volatile T *) (base_address + offset);
    }

    template<typename T = uint32_t, uint32_t offset = 0>
    T* get_ptr() {
        ASSERT_READABLE
        return reinterpret_cast<T*>(base_address + offset);
    }

    template<typename T = uint32_t>
    T* get_ptr_offset(uint32_t offset = 0) {
        ASSERT_READABLE
        return reinterpret_cast<T*>(base_address + offset);
    }

    // Read a std::array (offset defined at compile-time)
    template<typename T, size_t N, uint32_t offset = 0>
    std::array<T, N>& read_array() {
        auto p = get_ptr<std::array<T, N>, offset>();
        return *p;
    }

    // Read a std::array (offset defined at run-time)
    template<typename T, size_t N>
    std::array<T, N>& read_array_offset(uint32_t offset) {
        auto p = get_ptr_offset<std::array<T, N>>(offset);
        return *p;
    }

    ////////////////////////////////////////
    // Bit manipulation
    ////////////////////////////////////////

    // Set a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    void set_bit() {
        ASSERT_WRITABLE
        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) | (1 << index);
    }

    // Set a bit (offset and index defined at run-time)
    void set_bit_offset(uint32_t offset, uint32_t index) {
        ASSERT_WRITABLE
        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) | (1 << index);
    }

    // Clear a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    void clear_bit() {
        ASSERT_WRITABLE
        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) & ~(1 << index);
    }

    // Clear a bit (offset and index defined at run-time)
    void clear_bit_offset(uint32_t offset, uint32_t index) {
        ASSERT_WRITABLE
        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) & ~(1 << index);
    }

    // Toggle a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    void toggle_bit() {
        ASSERT_WRITABLE
        uintptr_t addr = base_address + sizeof(uint32_t) * offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) ^ (1 << index);
    }

    // Toggle a bit (offset and index defined at run-time)
    void toggle_bit_offset(uint32_t offset, uint32_t index) {
        ASSERT_WRITABLE
        uintptr_t addr = base_address + sizeof(uint32_t) * offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) ^ (1 << index);
    }

    // Read a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    bool read_bit() {
        ASSERT_READABLE
        return *((volatile uint32_t *) (base_address + offset)) & (1 << index);
    }

    // Read a bit (offset and index defined at run-time)
    bool read_bit_offset(uint32_t offset, uint32_t index) {
        ASSERT_READABLE
        return *((volatile uint32_t *) (base_address + offset)) & (1 << index);
    }

    enum Status {
        MEMMAP_CLOSED,       ///< Memory map closed
        MEMMAP_OPENED,       ///< Memory map opened
        MEMMAP_CANNOT_UMMAP, ///< Memory map cannot be unmapped
        MEMMAP_FAILURE       ///< Failure at memory mapping
    };

  private:
    int *fd;                    ///< /dev/mem file ID (Why is this a pointer ?)
    void *mapped_base;          ///< Map base address
    uintptr_t base_address;  ///< Virtual memory base address of the device
    int status;                 ///< Status
    int protection;
    uint32_t size;              ///< Map size in bytes
    uintptr_t phys_addr;        ///< Physical address
};

#endif // __DRIVERS_CORE_MEMORY_MAP_HPP__
