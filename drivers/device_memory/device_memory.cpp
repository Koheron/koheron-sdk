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