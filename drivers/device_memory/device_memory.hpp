/// DevMem driver
///
/// General purpose device memory interface for prototyping
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_DEVICE_MEMORY_HPP__
#define __DRIVERS_CORE_DEVICE_MEMORY_HPP__

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>

class DeviceMemory
{
  public:
    DeviceMemory(Klib::DevMem& dev_mem_);

    int add_memory_map(uint32_t device_addr, uint32_t map_size);
    uint32_t read(uint32_t mmap_idx, uint32_t offset);
    void write(uint32_t mmap_idx, uint32_t offset, uint32_t reg_val);

    #pragma tcp-server write_array arg{data} arg{len}
    void write_buffer(uint32_t mmap_idx, uint32_t offset,
                      const uint32_t *data, uint32_t len);

    #pragma tcp-server read_array arg{buff_size} 
    uint32_t* read_buffer(uint32_t mmap_idx, uint32_t offset, uint32_t buff_size);

    void set_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index);
    void clear_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index);
    void toggle_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index);

    void mask_and(uint32_t mmap_idx, uint32_t offset, uint32_t mask);
    void mask_or(uint32_t mmap_idx, uint32_t offset, uint32_t mask);

  private:
    Klib::DevMem& dev_mem;
};

#endif // __DRIVERS_CORE_DEVICE_MEMORY_HPP__