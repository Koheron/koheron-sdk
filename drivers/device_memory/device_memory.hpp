/// DeviceMemory driver
///
/// General purpose device memory interface for prototyping
///
/// (c) Koheron

#ifndef __DRIVERS_DEVICE_MEMORY_DEVICE_MEMORY_HPP__
#define __DRIVERS_DEVICE_MEMORY_DEVICE_MEMORY_HPP__

#include <drivers/lib/dev_mem.hpp>

class DeviceMemory
{
  public:
    DeviceMemory(Klib::DevMem& dvm_)
    : dvm(dvm_)
    {}

    int add_memory_map(uint32_t device_addr, uint32_t map_size) {
        return static_cast<int>(dvm.AddMemoryMap(device_addr, map_size));
    }

    uint32_t read(uint32_t mmap_idx, uint32_t offset) {
        return dvm.read32(mmap_idx, offset);
    }

    void write(uint32_t mmap_idx, uint32_t offset, uint32_t reg_val) {
        dvm.write32(mmap_idx, offset, reg_val);
    }

    void write_mask(uint32_t mmap_idx, uint32_t offset, uint32_t reg_val, uint32_t mask) {
        dvm.write32_mask(mmap_idx, offset, reg_val, mask);
    }

    #pragma tcp-server write_array arg{data} arg{len}
    void write_buffer(uint32_t mmap_idx, uint32_t offset,
                      const uint32_t *data, uint32_t len) {
        dvm.write_buff32(mmap_idx, offset, data, len);
    }

    #pragma tcp-server read_array arg{buff_size} 
    uint32_t* read_buffer(uint32_t mmap_idx, uint32_t offset, uint32_t buff_size) {
        return dvm.read_buff32(mmap_idx, offset);
    }

    void set_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index) {
        dvm.set_bit(mmap_idx, offset, index);
    }

    void clear_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index) {
        dvm.clear_bit(mmap_idx, offset, index);
    }
    
    void toggle_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index) {
        dvm.toggle_bit(mmap_idx, offset, index);
    }

    std::tuple<uintptr_t, int, uintptr_t, uint32_t, int>
    get_map_params(uint32_t mmap_idx) {
        return dvm.get_map_params(mmap_idx);
    }

  private:
    Klib::DevMem& dvm;
};

#endif // __DRIVERS_DEVICE_MEMORY_DEVICE_MEMORY_HPP__