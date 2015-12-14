/// (c) Koheron

#include "memory_map.hpp"

Klib::MemoryMap::MemoryMap(int *fd_, uint32_t dev_addr, uint32_t size_)
{
    size = size_;
    fd = fd_;

    if(dev_addr != 0x0) {
        mapped_base = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 
                           dev_addr & ~MAP_MASK(size) );

        if(mapped_base == (void *) -1) {
            fprintf(stderr, "Can't map the memory to user space.\n");
            close(*fd);
            status = MEMMAP_FAILURE;
            return;
        }

        status = MEMMAP_OPENED;

        // Device base address
        mapped_dev_base = (intptr_t)mapped_base + (dev_addr & MAP_MASK(size));
    } else {
        status = MEMMAP_CLOSED;
        mapped_base = NULL;
        mapped_dev_base = 0;
    }
}

Klib::MemoryMap::~MemoryMap()
{
    Unmap();
}

int Klib::MemoryMap::Unmap()
{
    if(status == MEMMAP_OPENED) {
        munmap(mapped_base, size);
        status = MEMMAP_CLOSED;
    }
	
    return 0;
}
