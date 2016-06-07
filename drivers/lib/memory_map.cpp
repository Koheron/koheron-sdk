/// (c) Koheron

#include "memory_map.hpp"

Klib::MemoryMap::MemoryMap(int *fd_, uintptr_t phys_addr_,
                           uint32_t size_, int permissions_)
: fd(fd_)
, mapped_base(nullptr)
, mapped_dev_base(0)
, status(MEMMAP_CLOSED)
, permissions(permissions_)
, size(size_)
, phys_addr(phys_addr_)
{
    if (phys_addr != 0x0) {
        int prot;
        switch (permissions) {
            case READ_WRITE:
                prot = PROT_READ | PROT_WRITE;
                break;
            case READ_ONLY:
                prot = PROT_READ;
                break;
            case WRITE_ONLY:
                prot = PROT_WRITE;
                break;
            default:
                prot = PROT_NONE;
                break;
        }
        
        mapped_base = mmap(0, size, prot, MAP_SHARED, *fd, phys_addr & ~MAP_MASK(size) );

        if (mapped_base == (void *) -1) {
            fprintf(stderr, "Can't map the memory to user space.\n");
            close(*fd);
            status = MEMMAP_FAILURE;
            return;
        }

        status = MEMMAP_OPENED;
        mapped_dev_base = (uintptr_t)mapped_base + (phys_addr & MAP_MASK(size));
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
