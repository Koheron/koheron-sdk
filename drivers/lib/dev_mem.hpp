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
#include <assert.h>

extern "C" {
    #include <fcntl.h>
    #include <sys/mman.h>
}

namespace kserver {class KServer;}

#include "memory_map.hpp"

/// ID of a memory map
typedef uint32_t MemMapID;

class MemMapIdPool
{
  public:
    MemMapIdPool() : reusable_ids(0) {};

    MemMapID get_id(unsigned int num_maps);
    void release_id(MemMapID id);
  private:
    std::vector<MemMapID> reusable_ids;
};

#define ASSERT_WRITABLE assert((mem_maps.at(id)->get_protection() & PROT_WRITE) == PROT_WRITE);
#define ASSERT_READABLE assert((mem_maps.at(id)->get_protection() & PROT_READ) == PROT_READ);

/// Device memory manager
/// A memory maps factory
class DevMem
{
  public:
    DevMem(kserver::KServer *kserver_, uintptr_t addr_limit_down_=0x0, uintptr_t addr_limit_up_=0x0);
    ~DevMem();

    /// Open the /dev/mem driver
    int open();

    /// Close all the memory maps
    /// @return 0 if succeed, -1 else
    int close();

    /// Current number of memory maps
    static unsigned int num_maps;

    int resize(MemMapID id, uint32_t length) {return mem_maps.at(id)->resize(length);}

    /// Create a new memory map
    /// @addr Base address of the map
    /// @size Size of the map
    /// @protection Access protection
    /// @return An ID to the created map, or -1 if an error occured
    MemMapID add_memory_map(uintptr_t addr, uint32_t size, int protection = PROT_READ|PROT_WRITE);

    /// Remove a memory map
    /// @id ID of the memory map to be removed
    void rm_memory_map(MemMapID id);

    /// Remove all the memory maps
    void remove_all();

    uintptr_t get_base_addr(MemMapID id) {return mem_maps.at(id)->get_base_addr();}
    int get_status(MemMapID id)         {return mem_maps.at(id)->get_status();}

    std::tuple<uintptr_t, int, uintptr_t, uint32_t, int>
    get_map_params(MemMapID id) {
        return mem_maps.at(id)->get_params();
    }

    /// Return 1 if a memory map failed
    int IsFailed();

    bool is_ok() {return !IsFailed();}

    // Write

    void write32(MemMapID id, uint32_t offset, uint32_t value) {
        ASSERT_WRITABLE
        *(volatile uintptr_t *) (get_base_addr(id) + offset) = value;
    }

    void write_buff32(MemMapID id, uint32_t offset,
                      const uint32_t *data_ptr, uint32_t buff_size) {
        ASSERT_WRITABLE
        uintptr_t addr = get_base_addr(id) + offset;
        for (uint32_t i=0; i < buff_size; i++)
            *(volatile uintptr_t *) (addr + sizeof(uint32_t) * i) = data_ptr[i];
    }

    template<size_t N>
    void write_buff32(MemMapID id, uint32_t offset, const std::array<uint32_t, N> arr) {
        write_buff32(id, offset, arr.data(), N);
    }

    // Read

    template<typename T>
    T* read_buffer(MemMapID id, uint32_t offset) {
        ASSERT_READABLE
        return reinterpret_cast<T*>(get_base_addr(id) + offset);
    }

    uint32_t* read_buff32(MemMapID id, uint32_t offset) {
        return read_buffer<uint32_t>(id, offset);
    }

    template<typename T, uint32_t offset = 0>
    T* get_buffer_ptr(MemMapID id) {
        ASSERT_READABLE
        return reinterpret_cast<T*>(get_base_addr(id) + offset);
    }

    template<typename T, size_t N, uint32_t offset = 0>
    std::array<T, N>& read_buffer(MemMapID id) {
        uintptr_t ptr = get_base_addr(id) + offset;
        auto p = reinterpret_cast<std::array<T, N>*>(ptr);
        assert(p->data() == (const T*)ptr);
        return *p;
    }

    template<uint32_t offset = 0>
    uint32_t* read_buff32(MemMapID id) {
        return get_buffer_ptr<uint32_t, offset>(id);
    }

    uint32_t read32(MemMapID id, uint32_t offset) {
        ASSERT_READABLE
        return *(volatile uintptr_t *) (get_base_addr(id) + offset);
    }

    // Bits

    void set_bit(MemMapID id, uint32_t offset, uint32_t index) {
        ASSERT_WRITABLE
        uintptr_t addr = get_base_addr(id) + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) | (1 << index);
    }

    void clear_bit(MemMapID id, uint32_t offset, uint32_t index) {
        ASSERT_WRITABLE
        uintptr_t addr = get_base_addr(id) + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) & ~(1 << index);
    }

    void toggle_bit(MemMapID id, uint32_t offset, uint32_t index) {
        ASSERT_WRITABLE
        uintptr_t addr = get_base_addr(id) + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) ^ (1 << index);
    }

    bool read_bit(MemMapID id, uint32_t offset, uint32_t index) {
        ASSERT_READABLE
        return *((volatile uintptr_t *) (get_base_addr(id) + offset)) & (1 << index);
    }

    void write32_mask(MemMapID id, uint32_t offset, uint32_t value, uint32_t mask) {
        ASSERT_WRITABLE
        uintptr_t addr = get_base_addr(id) + offset;
        *(volatile uintptr_t *) addr = (*((volatile uintptr_t *) addr) & ~mask) | (value & mask);
    }

    /// True if the /dev/mem device is open
    bool IsOpen() const {return is_open;}

  private:
    kserver::KServer *kserver;
    int fd;         ///< /dev/mem file ID
    bool is_open;   ///< True if /dev/mem open
    
    /// Limit addresses
    uintptr_t addr_limit_down;
    uintptr_t addr_limit_up;
    bool is_forbidden_address(uintptr_t addr);
    MemMapID create_memory_map(uintptr_t addr, uint32_t size, int protection);

    std::map<MemMapID, std::unique_ptr<MemoryMap>> mem_maps;
    MemMapIdPool id_pool;
};

#endif // __DRIVERS_LIB_DEV_MEM_HPP__
