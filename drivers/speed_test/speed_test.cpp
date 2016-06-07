/// (c) Koheron

#include "speed_test.hpp"

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


SpeedTest::SpeedTest(Klib::DevMem& dvm_)
: dvm(dvm_)
, data_decim(0)
, data_all_int(0)
{
    config_map = dvm.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
    status_map = dvm.AddMemoryMap(STATUS_ADDR, STATUS_RANGE, PROT_READ);
    adc_1_map  = dvm.AddMemoryMap(ADC1_ADDR, ADC1_RANGE);
    adc_2_map  = dvm.AddMemoryMap(ADC2_ADDR, ADC2_RANGE);
    rambuf_map = dvm.AddMemoryMap(RAMBUF_ADDR, RAMBUF_RANGE);

    raw_data_1 = reinterpret_cast<uint32_t*>(dvm.GetBaseAddr(adc_1_map));
    raw_data_2 = reinterpret_cast<uint32_t*>(dvm.GetBaseAddr(adc_2_map));
    rambuf_data = reinterpret_cast<float*>(dvm.GetBaseAddr(rambuf_map));

    mmap_buf = mmap(NULL, 16384*4, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);

    data_all_int = std::vector<uint32_t>(WFM_SIZE, 0);
}

// http://stackoverflow.com/questions/12276675/modulus-with-negative-numbers-in-c
inline long long int mod(long long int k, long long int n) 
{
    return ((k %= n) < 0) ? k+n : k;
}

// Read the two channels in raw format
std::array<float, 2*WFM_SIZE>& SpeedTest::read_raw_all()
{
    dvm.set_bit(config_map, ADDR_OFF, 1);

    for(unsigned int i=0; i<WFM_SIZE; i++) {
        data_all[i] = raw_data_1[i];
        data_all[i + WFM_SIZE] = raw_data_2[i];
    }

    dvm.clear_bit(config_map, ADDR_OFF, 1);
    return data_all;
}
