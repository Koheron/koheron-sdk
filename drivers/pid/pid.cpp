/// (c) Koheron

#include "pid.hpp"

#include <thread>
#include <chrono>

Pid::Pid(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
{
    status = CLOSED;
}

int Pid::Open()
{
    if (status == CLOSED) {
        auto ids = dev_mem.RequestMemoryMaps(mem_regions(
            Klib::MemoryRegion({ CONFIG_ADDR, CONFIG_RANGE }),
            Klib::MemoryRegion({ STATUS_ADDR, STATUS_RANGE }),
            Klib::MemoryRegion({ FIFO_ADDR  , FIFO_RANGE   })
        ));

        if (dev_mem.CheckMapIDs(ids) < 0) {
            status = FAILED;
            return -1;
        }

        config_map = ids[0];
        status_map = ids[1];
        fifo_map   = ids[2];
        
        fifo.set_address(dev_mem.GetBaseAddr(fifo_map));
        status = OPENED;
    }
    
    return 0;
}

// Read the peak data stream

void Pid::fifo_start_acquisition(uint32_t acq_period)
{
    fifo.start_acquisition(acq_period);
}

void Pid::fifo_stop_acquisition()
{
    fifo.stop_acquisition();
}

bool Pid::fifo_get_acquire_status()
{
    return fifo.get_acquire_status();
}

uint32_t Pid::get_fifo_buffer_length()
{
    return fifo.get_buffer_length();
}

std::vector<uint32_t>& Pid::get_fifo_data()
{
    return fifo.get_data();
}

uint32_t Pid::get_fifo_length()
{
    return fifo.get_fifo_length();
}
