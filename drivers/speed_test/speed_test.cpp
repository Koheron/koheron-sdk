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


SpeedTest::SpeedTest(DevMem& dvm_)
: dvm(dvm_)
, cfg(dvm.get<CONFIG_MEM>())
, sts(dvm.get<STATUS_MEM>())
, adc_map(dvm.get<ADC_MEM>())
, rambuf_map(dvm.get<RAMBUF_MEM>())
, data_decim(0)
, data_all_int(0)
{
    raw_data_1 = adc_map.get_ptr<uint32_t>(0);
    raw_data_2 = adc_map.get_ptr<uint32_t>(1);
    rambuf_data = rambuf_map.get_ptr<float>();

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
    cfg.set_bit<ADDR_OFF, 1>();

    for (unsigned int i=0; i<WFM_SIZE; i++) {
        data_all[i] = raw_data_1[i];
        data_all[i + WFM_SIZE] = raw_data_2[i];
    }

    cfg.clear_bit<ADDR_OFF, 1>();
    return data_all;
}