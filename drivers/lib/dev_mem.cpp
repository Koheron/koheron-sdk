/// (c) Koheron

#include "dev_mem.hpp"

#include <core/kserver.hpp>

DevMem::DevMem(kserver::KServer *kserver_)
: kserver(kserver_)
, failed_maps(0)
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

    create_maps<addresses::count>();

    if (! failed_maps.empty())
        return -1;

    return fd;
}

template<MemMapID id>
int DevMem::add_memory_map()
{
    auto mem_map = std::make_unique<MemoryMap<id>>(&fd);

    if (mem_map->get_status() != MemoryMap<id>::MEMMAP_OPENED) {
        fprintf(stderr, "Can't open memory map id = %u\n", id);
        return -1;
    }

    mem_maps[id] = static_cast<std::unique_ptr<MemoryMapBase>>(std::move(mem_map));
    return 0;
}
