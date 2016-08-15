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

typedef uint32_t MemMapID; /// ID of a memory map

/// Memory blocks array
template<size_t n_blocks>
using memory_blocks = std::array<std::array<uint32_t, 2>, n_blocks>;

class MemMapIdPool
{
  public:
    MemMapIdPool() : reusable_ids(0) {};

    MemMapID get_id(unsigned int num_maps);

    void release_id(MemMapID id) {
        reusable_ids.push_back(id);
    }

  private:
    std::vector<MemMapID> reusable_ids;
};

class DevMem
{
  public:
    DevMem(kserver::KServer *kserver_, uintptr_t addr_limit_down_=0x0, uintptr_t addr_limit_up_=0x0);
    ~DevMem();

    int open();

    static unsigned int num_maps; // Current number of memory maps

    int resize(MemMapID id, uint32_t length) {return mem_maps.at(id)->resize(length);}

    // Create a new memory map
    MemoryMap* add_memory_map(uintptr_t addr, uint32_t size, int protection = PROT_READ|PROT_WRITE);

    template<size_t n_blocks>
    std::array<MemoryMap*, n_blocks> add_memory_blocks(memory_blocks<n_blocks> mem_blocks,
                                                     int protection = PROT_READ|PROT_WRITE)
    {
        std::array<MemoryMap*, n_blocks> maps;

        for (uint32_t i=0; i<n_blocks; i++)
            maps[i] = add_memory_map(mem_blocks[i][0], mem_blocks[i][1], protection);

        return maps;
    }

    // Remove a memory map
    void rm_memory_map(MemMapID id);

    // Remove all the memory maps
    void remove_all();

    uintptr_t get_base_addr(MemMapID id) {return mem_maps.at(id)->get_base_addr();}
    int get_status(MemMapID id)          {return mem_maps.at(id)->get_status();}

    std::tuple<uintptr_t, int, uintptr_t, uint32_t, int>
    get_map_params(MemMapID id) {
        return mem_maps.at(id)->get_params();
    }

    // ////////////////////////////////////////
    // // Write functions
    // ////////////////////////////////////////

    // // Write a register (offset defined at compile-time)
    // template<uint32_t offset, typename T = uint32_t>
    // void write(MemMapID id, T value) {
    //     ASSERT_WRITABLE
    //     *(volatile T *) (get_base_addr(id) + offset) = value;
    // }

    // // Write a register (offset defined at run-time)
    // template<typename T = uint32_t>
    // void write_offset(MemMapID id, uint32_t offset, T value) {
    //     ASSERT_WRITABLE
    //     *(volatile T *) (get_base_addr(id) + offset) = value;
    // }

    // template<typename T = uint32_t, uint32_t offset = 0>
    // void set_ptr(MemMapID id, const T *data_ptr, uint32_t buff_size) {
    //     ASSERT_WRITABLE
    //     uintptr_t addr = get_base_addr(id) + offset;
    //     for (uint32_t i=0; i < buff_size; i++)
    //         *(volatile T *) (addr + sizeof(T) * i) = data_ptr[i];
    // }

    // template<typename T = uint32_t>
    // void set_ptr_offset(MemMapID id, uint32_t offset, const T *data_ptr, uint32_t buff_size) {
    //     ASSERT_WRITABLE
    //     uintptr_t addr = get_base_addr(id) + offset;
    //     for (uint32_t i=0; i < buff_size; i++)
    //         *(volatile T *) (addr + sizeof(T) * i) = data_ptr[i];
    // }

    // // Write a std::array (offset defined at compile-time)
    // template<typename T, size_t N, uint32_t offset = 0>
    // void write_array(MemMapID id, const std::array<T, N> arr) {
    //     set_ptr<T, offset>(id, arr.data(), N);
    // }

    // // Write a std::array (offset defined at run-time)
    // template<typename T, size_t N>
    // void write_array_offset(MemMapID id, uint32_t offset, const std::array<T, N> arr) {
    //     set_ptr_offset<T>(id, offset, arr.data(), N);
    // }

    // ////////////////////////////////////////
    // // Read functions
    // ////////////////////////////////////////

    // // Read a register (offset defined at compile-time)
    // template<uint32_t offset, typename T = uint32_t>
    // T read(MemMapID id) {
    //     ASSERT_READABLE
    //     return *(volatile T *) (get_base_addr(id) + offset);
    // }

    // // Read a register (offset defined at run-time)
    // template<typename T = uint32_t>
    // T read_offset(MemMapID id, uint32_t offset) {
    //     ASSERT_READABLE
    //     return *(volatile T *) (get_base_addr(id) + offset);
    // }

    // template<typename T = uint32_t, uint32_t offset = 0>
    // T* get_ptr(MemMapID id) {
    //     ASSERT_READABLE
    //     return reinterpret_cast<T*>(get_base_addr(id) + offset);
    // }

    // template<typename T = uint32_t>
    // T* get_ptr_offset(MemMapID id, uint32_t offset = 0) {
    //     ASSERT_READABLE
    //     return reinterpret_cast<T*>(get_base_addr(id) + offset);
    // }

    // // Read a std::array (offset defined at compile-time)
    // template<typename T, size_t N, uint32_t offset = 0>
    // std::array<T, N>& read_array(MemMapID id) {
    //     auto p = get_ptr<std::array<T, N>, offset>(id);
    //     return *p;
    // }

    // // Read a std::array (offset defined at run-time)
    // template<typename T, size_t N>
    // std::array<T, N>& read_array_offset(MemMapID id, uint32_t offset) {
    //     auto p = get_ptr_offset<std::array<T, N>>(id, offset);
    //     return *p;
    // }

    

  private:
    kserver::KServer *kserver;
    int fd;         // /dev/mem file ID
    bool is_open;   // True if /dev/mem open
    
    // Limit addresses
    uintptr_t addr_limit_down;
    uintptr_t addr_limit_up;
    bool is_forbidden_address(uintptr_t addr);
    MemMapID create_memory_map(uintptr_t addr, uint32_t size, int protection);

    std::map<MemMapID, std::unique_ptr<MemoryMap>> mem_maps;
    MemMapIdPool id_pool;
};

#endif // __DRIVERS_LIB_DEV_MEM_HPP__
