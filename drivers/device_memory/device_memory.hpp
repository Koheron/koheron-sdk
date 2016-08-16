/// DeviceMemory driver
///
/// General purpose device memory interface for prototyping
///
/// (c) Koheron

#ifndef __DRIVERS_DEVICE_MEMORY_DEVICE_MEMORY_HPP__
#define __DRIVERS_DEVICE_MEMORY_DEVICE_MEMORY_HPP__

#include <tuple>
#include <string>

#include <drivers/lib/dev_mem.hpp>
#include <drivers/addresses.hpp>

class DeviceMemory
{
  public:
    DeviceMemory(DevMem& dvm_)
    : dvm(dvm_)
    {}

    uint32_t read(uint32_t mmap_idx, uint32_t offset) {
        return dvm[mmap_idx].read_offset(offset);
    }

    void write(uint32_t mmap_idx, uint32_t offset, uint32_t reg_val) {
        dvm[mmap_idx].write_offset(offset, reg_val);
    }

    void write_mask(uint32_t mmap_idx, uint32_t offset, uint32_t mask, uint32_t reg_val) {
        dvm[mmap_idx].write_mask_offset(offset, mask, reg_val);
    }

    #pragma tcp-server write_array arg{data} arg{len}
    void write_buffer(uint32_t mmap_idx, uint32_t offset,
                      const uint32_t *data, uint32_t len) {
        dvm[mmap_idx].set_ptr_offset(offset, data, len);
    }

    #pragma tcp-server read_array arg{buff_size}
    uint32_t* read_buffer(uint32_t mmap_idx, uint32_t offset, uint32_t buff_size) {
        return dvm[mmap_idx].get_ptr_offset(offset);
    }

    void set_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index) {
        dvm[mmap_idx].set_bit_offset(offset, index);
    }

    void clear_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index) {
        dvm[mmap_idx].clear_bit_offset(offset, index);
    }
    
    void toggle_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index) {
        dvm[mmap_idx].toggle_bit_offset(offset, index);
    }

    bool read_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index) {
        return dvm[mmap_idx].read_bit_offset(offset, index);
    }

    std::tuple<uintptr_t, int, uintptr_t, uint32_t, int>
    get_map_params(uint32_t mmap_idx) {
        return dvm.get_map_params(mmap_idx);
    }

    std::string get_instrument_config() {
        return CFG_JSON;
    }

  private:
    DevMem& dvm;
};

#endif // __DRIVERS_DEVICE_MEMORY_DEVICE_MEMORY_HPP__
