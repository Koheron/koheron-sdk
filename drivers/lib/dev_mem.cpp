/// (c) Koheron

#include "dev_mem.hpp"

#include <core/kserver.hpp>

DevMem::DevMem(kserver::KServer *kserver_)
: kserver(kserver_)
{
    fd = -1;
    is_open = 0;
}

DevMem::~DevMem()
{
    close(fd);
}

int DevMem::open()
{
    if (fd == -1) {
        fd = ::open("/dev/mem", O_RDWR | O_SYNC);

         if (fd == -1) {
            fprintf(stderr, "Can't open /dev/mem\n");
            return -1;
        }
        
        is_open = 1;
    }

    if (add_memory_maps() < 0)
        return -1;

    return fd;
}

int DevMem::add_memory_maps()
{
    int err = 0;
    for (MemMapID id=0; id<addresses::count; id++)
        if (add_memory_map(id) < 0)
            err = -1;
    return err;
}

int DevMem::add_memory_map(MemMapID id)
{
    auto mem_map = std::make_unique<MemoryMap>(&fd, addresses::get_base_addr(id),
                        addresses::get_range(id), addresses::get_protection(id));

    if (mem_map->get_status() != MemoryMap::MEMMAP_OPENED) {
        fprintf(stderr, "Can't open memory map id = %u\n", id);
        return -1;
    }

    mem_maps[id] = std::move(mem_map);
    return 0;
}
