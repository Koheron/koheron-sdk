/// (c) Koheron

#include "device_memory.hpp"

DeviceMemory::DeviceMemory(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
{}

int DeviceMemory::add_memory_map(uint32_t device_addr, uint32_t map_size)
{
    auto ids = dev_mem.RequestMemoryMaps<1>({{
        { device_addr, map_size }
    }});

    if (dev_mem.CheckMapIDs(ids) < 0)
        return -1;

    return static_cast<int>(ids[0]);
}

uint32_t DeviceMemory::read(uint32_t mmap_idx, uint32_t offset)
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(mmap_idx) + offset);
}

void DeviceMemory::write(uint32_t mmap_idx, uint32_t offset, uint32_t reg_val)
{
    Klib::WriteReg32(dev_mem.GetBaseAddr(mmap_idx) + offset, reg_val);
}

void DeviceMemory::write_buffer(uint32_t mmap_idx, uint32_t offset, 
                                const uint32_t *data, uint32_t len)
{
    Klib::WriteBuff32(dev_mem.GetBaseAddr(mmap_idx) + offset, data, len);
}

uint32_t* DeviceMemory::read_buffer(uint32_t mmap_idx, uint32_t offset,
                                    uint32_t buff_size)
{
    return reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(mmap_idx) + offset);
}

void DeviceMemory::set_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index)
{
    Klib::SetBit(dev_mem.GetBaseAddr(mmap_idx) + offset, index);
}

void DeviceMemory::clear_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index)
{
    Klib::ClearBit(dev_mem.GetBaseAddr(mmap_idx) + offset, index);
}

void DeviceMemory::toggle_bit(uint32_t mmap_idx, uint32_t offset, uint32_t index)
{
    Klib::ToggleBit(dev_mem.GetBaseAddr(mmap_idx) + offset, index);
}

void DeviceMemory::mask_and(uint32_t mmap_idx, uint32_t offset, uint32_t mask)
{
    Klib::MaskAnd(dev_mem.GetBaseAddr(mmap_idx) + offset, mask);
}

void DeviceMemory::mask_or(uint32_t mmap_idx, uint32_t offset, uint32_t mask)
{
    Klib::MaskOr(dev_mem.GetBaseAddr(mmap_idx) + offset, mask);
}