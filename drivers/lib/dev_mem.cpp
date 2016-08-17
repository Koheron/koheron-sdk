/// (c) Koheron

#include "dev_mem.hpp"

#include <core/kserver.hpp>
#include <core/kserver_syslog.hpp>

DevMem::DevMem(kserver::KServer *kserver_)
: kserver(kserver_)
, failed_maps(0)
{
    fd = -1;
}

DevMem::~DevMem()
{
    close(fd);
}

int DevMem::open()
{
    fd = ::open("/dev/mem", O_RDWR | O_SYNC);

     if (fd == -1) {
        kserver->syslog.print(kserver::SysLog::PANIC,
                              "Can't open /dev/mem\n");
        return -1;
    }

    create_maps<addresses::count>();

    if (! failed_maps.empty())
        return -1;

    return fd;
}

template<MemMapID id>
int DevMem::add_memory_map()
{
    auto mem_map = std::make_unique<MemoryMap<id>>(fd);

    if (mem_map->get_status() != MemoryMap<id>::MEMMAP_OPENED) {
        kserver->syslog.print(kserver::SysLog::CRITICAL,
                              "Can't open memory map id = %u\n", id);
        return -1;
    }

    mem_maps[id] = static_cast<std::unique_ptr<MemoryMapBase>>(std::move(mem_map));
    return 0;
}
