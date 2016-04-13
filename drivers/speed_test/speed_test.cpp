/// (c) Koheron

#include "speed_test.hpp"
#include <string.h>
#include <thread>
#include <chrono>

//http://es.codeover.org/questions/34888683/arm-neon-memcpy-optimized-for-uncached-memory
void mycopy(volatile unsigned char *dst, volatile unsigned char *src, int sz)
{
    if (sz & 63) {
        sz = (sz & -64) + 64;
    }
    asm volatile (
        "NEONCopyPLD:                          \n"
        "    VLDM %[src]!,{d0-d7}                 \n"
        "    VSTM %[dst]!,{d0-d7}                 \n"
        "    SUBS %[sz],%[sz],#0x40                 \n"
        "    BGT NEONCopyPLD                  \n"
        : [dst]"+r"(dst), [src]"+r"(src), [sz]"+r"(sz) : : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "cc", "memory");
}


SpeedTest::SpeedTest(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
, data_decim(0)
, data_all_int(0)
{
    status = CLOSED;
}
 
SpeedTest::~SpeedTest()
{
    Close();
}

int SpeedTest::Open(uint32_t waveform_size_)
{
    // Reopening
    if(status == OPENED) {
        Close();
    }

    if(status == CLOSED) {
 
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
        
        adc_1_map = dev_mem.AddMemoryMap(ADC1_ADDR, ADC1_RANGE);
        
        if (static_cast<int>(adc_1_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        adc_2_map = dev_mem.AddMemoryMap(ADC2_ADDR, ADC2_RANGE);
        
        if (static_cast<int>(adc_2_map) < 0) {
            status = FAILED;
            return -1;
        }

        rambuf_map = dev_mem.AddMemoryMap(RAMBUF_ADDR, RAMBUF_RANGE);
        
        if (static_cast<int>(rambuf_map) < 0) {
            status = FAILED;
            return -1;
        }

        raw_data_1 = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_1_map));
        raw_data_2 = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_2_map));
        rambuf_data = reinterpret_cast<float*>(dev_mem.GetBaseAddr(rambuf_map));

        mmap_buf = mmap(NULL, 16384*4, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);

        data_all_int = std::vector<uint32_t>(WFM_SIZE, 0);

        status = OPENED;
    }
    
    return 0;
}

void SpeedTest::Close()
{
    if(status == OPENED) {
        dev_mem.RmMemoryMap(config_map);
        dev_mem.RmMemoryMap(status_map);
        dev_mem.RmMemoryMap(adc_1_map);
        dev_mem.RmMemoryMap(adc_2_map);
        status = CLOSED;
    }
}

// http://stackoverflow.com/questions/12276675/modulus-with-negative-numbers-in-c
inline long long int mod(long long int k, long long int n) 
{
    return ((k %= n) < 0) ? k+n : k;
}

// Read the two channels in raw format
std::array<float, 2*WFM_SIZE>& SpeedTest::read_raw_all()
{
    Klib::SetBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    //_wait_for_acquisition();
    for(unsigned int i=0; i<WFM_SIZE; i++) {
        data_all[i] = raw_data_1[i];
        data_all[i + WFM_SIZE] = raw_data_2[i];
    }
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    return data_all;
}

// Return zeros (does not perform FPGA memory access)
std::array<float, 2*WFM_SIZE>& SpeedTest::read_zeros()
{
    return data_zeros;
}

// Read data in RAM buffer
float* SpeedTest::read_rambuf()
{
    return rambuf_data;
}

// Read data in RAM buffer (with copy)
std::array<float, 2*WFM_SIZE>& SpeedTest::read_rambuf_memcpy()
{
    memcpy((unsigned char*)rambuf_copy.data(), (unsigned char*)rambuf_data, 2*WFM_SIZE*sizeof(float));
    return rambuf_copy;
}

// Read data in RAM buffer (with optimized copy)
std::array<float, 2*WFM_SIZE>& SpeedTest::read_rambuf_mycopy()
{
    mycopy((unsigned char*)rambuf_copy.data(), (unsigned char*)rambuf_data, 2*WFM_SIZE*sizeof(float));
    return rambuf_copy;
}

// Read data in RAM buffer
float* SpeedTest::read_mmapbuf_nocopy()
{
    return (float*)mmap_buf;
}

// Read data in RAM buffer
float* SpeedTest::read_rambuf_mmap_memcpy()
{
    memcpy(mmap_buf, rambuf_data, 2*WFM_SIZE*sizeof(float));
    return (float*)mmap_buf;
}

