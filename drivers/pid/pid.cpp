/// (c) Koheron

#include "pid.hpp"

#include <thread>
#include <chrono>

Pid::Pid(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
, fifo_data(0)
{
    status = CLOSED;
}

int Pid::Open()
{
    if (status == CLOSED) {
        auto ids = dev_mem.RequestMemoryMaps<3>({{
            { CONFIG_ADDR, CONFIG_RANGE },
            { STATUS_ADDR, STATUS_RANGE },
            { FIFO_ADDR, FIFO_RANGE }
        }});

        if (dev_mem.CheckMapIDs(ids) < 0) {
            status = FAILED;
            return -1;
        }

        config_map = ids[0];
        status_map = ids[1];
        fifo_map = ids[2];
        status = OPENED;
    }
    
    return 0;
}

// Read the peak data stream

// Read the data stream
// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf

uint32_t Pid::get_fifo_occupancy()
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(fifo_map)+RDFO_OFF);
}

uint32_t Pid::get_fifo_length()
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(fifo_map)+RLR_OFF);
}

void Pid::reset_fifo()
{
    return Klib::WriteReg32(dev_mem.GetBaseAddr(fifo_map)+RDFR_OFF, 0x000000A5);
}

std::vector<uint32_t>& Pid::get_fifo_data(uint32_t n_pts) 
{
    fifo_data.resize(n_pts);
    for(unsigned int i=0; i < fifo_data.size(); i++)
        fifo_data[i] = Klib::ReadReg32(dev_mem.GetBaseAddr(fifo_map)+RDFD_OFF);
    return fifo_data;
}
