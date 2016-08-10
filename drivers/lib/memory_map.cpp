/// (c) Koheron

#include "memory_map.hpp"

Klib::MemoryMap::MemoryMap(int *fd_, uintptr_t phys_addr_,
                           uint32_t size_, int protection_)
: fd(fd_)
, mapped_base(nullptr)
, mapped_dev_base(0)
, status(MEMMAP_CLOSED)
, protection(protection_)
, size(size_)
, phys_addr(phys_addr_)
{
    mapped_base = mmap(0, size, protection, MAP_SHARED, *fd, phys_addr & ~MAP_MASK(size) );

    if (mapped_base == (void *) -1) {
        fprintf(stderr, "Can't map the memory to user space.\n");
        close(*fd);
        status = MEMMAP_FAILURE;
        return;
    }

    status = MEMMAP_OPENED;
    mapped_dev_base = (uintptr_t)mapped_base + (phys_addr & MAP_MASK(size));
}

Klib::MemoryMap::~MemoryMap()
{
    unmap();
}

int Klib::MemoryMap::unmap()
{
    if (status == MEMMAP_OPENED) {
        munmap(mapped_base, size);
        status = MEMMAP_CLOSED;
    }

    return 0;
}

int Klib::MemoryMap::resize(uint32_t length)
{
    void *new_virt_addr = mremap((void *)mapped_dev_base, size, length, 0);

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
