/// (c) Koheron

#include "memory_map.hpp"

Klib::MemoryMap::MemoryMap(int *fd_, uintptr_t phys_addr_, uint32_t size_)
{
    size = size_;
    fd = fd_;
    phys_addr = phys_addr_;

    if (phys_addr != 0x0) {
        mapped_base = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 
                           phys_addr & ~MAP_MASK(size) );

        if (mapped_base == (void *) -1) {
            fprintf(stderr, "Can't map the memory to user space.\n");
            close(*fd);
            status = MEMMAP_FAILURE;
            return;
        }

        status = MEMMAP_OPENED;

        // Device base address
        mapped_dev_base = (uintptr_t)mapped_base + (phys_addr & MAP_MASK(size));
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
    if (status == MEMMAP_OPENED) {
        munmap(mapped_base, size);
        status = MEMMAP_CLOSED;
    }
	
    return 0;
}

int Klib::MemoryMap::Resize(uint32_t length)
{
    void *new_virt_addr = mremap((void *)mapped_dev_base, size, length, 0);

    // printf("old virt addr = %lu\n", (uintptr_t)mapped_dev_base);
    // printf("new virt addr = %lu\n", (uintptr_t)new_virt_addr);

    if (new_virt_addr == (void *) -1) {
        fprintf(stderr, "Can't resize memory map.\n");
        return -1;
    }

    if ((uintptr_t)new_virt_addr != mapped_dev_base) {
        fprintf(stderr, "New address shifted during resizing.\n");
        return -1;
    }

    size = length;
    return 0;
}