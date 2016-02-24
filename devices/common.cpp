/// (c) Koheron

#include "common.hpp"

Common::Common(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
{
    status = CLOSED;
}

Common::~Common()
{
    Close();
}

int Common::Open()
{  
    if (status == CLOSED) {    
        // Initializes memory maps
        config_map = dev_mem.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
        
        if (static_cast<int>(config_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        status_map = dev_mem.AddMemoryMap(STATUS_ADDR, STATUS_RANGE);
        
        if (static_cast<int>(status_map) < 0) {
            status = FAILED;
            return -1;
        }  
       
        status = OPENED;
    }
    
    return 0;
}

void Common::Close()
{
    if(status == OPENED) {
        dev_mem.RmMemoryMap(config_map);
        dev_mem.RmMemoryMap(status_map);
        status = CLOSED;
    }
}

std::array<uint32_t, BITSTREAM_ID_SIZE> Common::get_bitstream_id()
{
    for (uint32_t i=0; i<bitstream_id.size(); i++)
        bitstream_id[i] = Klib::ReadReg32(dev_mem.GetBaseAddr(status_map) 
                                          + BITSTREAM_ID_OFF + 4*i);
        
    return bitstream_id;
}

