/// (c) Koheron

#include "pid.hpp"

#include <thread>
#include <chrono>

Pid::Pid(Klib::DevMem& dvm_)
: dvm(dvm_)
{
    status = CLOSED;

    config_map = dvm.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
    status_map = dvm.AddMemoryMap(STATUS_ADDR, STATUS_RANGE, Klib::MemoryMap::READ_ONLY);
    fifo_map = dvm.AddMemoryMap(FIFO_ADDR, FIFO_RANGE);

    if (dvm.CheckMaps(config_map, status_map, fifo_map) < 0)
        status = FAILED;

    fifo.set_address(dvm.GetBaseAddr(fifo_map));
    status = OPENED;
}
