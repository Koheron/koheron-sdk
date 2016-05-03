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

  private:
  	Klib::DevMem& dev_mem;
};

#endif // __DRIVERS_CORE_DEVICE_MEMORY_HPP__