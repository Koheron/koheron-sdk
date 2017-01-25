/// SpeedTest driver
///
/// (c) Koheron

#ifndef __DRIVERS_SPEED_TEST_HPP__
#define __DRIVERS_SPEED_TEST_HPP__

#include <vector>
#include <cstring>

#include <context.hpp>

#define WFM_SIZE 32 * 1024 / sizeof(float)

//http://es.codeover.org/questions/34888683/arm-neon-memcpy-optimized-for-uncached-memory
static void mycopy(volatile unsigned char *dst, volatile unsigned char *src, int sz) {
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

class SpeedTest
{
  public:
    SpeedTest(Context& ctx)
    : rambuf_map(ctx.mm.get<mem::rambuf>())
    {
        rambuf_data = rambuf_map.get_ptr<float>();
        mmap_buf = mmap(NULL, 16384*4, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        data_zeros.fill(0);
    }

    // Return zeros (does not perform FPGA memory access)
    std::array<float, 2*WFM_SIZE>& read_zeros() {
        return data_zeros;
    }

    // Read data in RAM buffer
    std::array<float, 2*WFM_SIZE>& read_rambuf() {
        auto p = reinterpret_cast<std::array<float, 2*WFM_SIZE>*>(rambuf_data);
        return *p;
    }

    // Read data in RAM buffer (with copy)
    std::array<float, 2*WFM_SIZE>& read_rambuf_memcpy() {
        memcpy((unsigned char*)rambuf_copy.data(), (unsigned char*)rambuf_data, 2*WFM_SIZE*sizeof(float));
        return rambuf_copy;
    }

    // Read data in RAM buffer (with optimized copy)
    std::array<float, 2*WFM_SIZE>& read_rambuf_mycopy() {
        mycopy((unsigned char*)rambuf_copy.data(), (unsigned char*)rambuf_data, 2*WFM_SIZE*sizeof(float));
        return rambuf_copy;
    }

    // Read data in RAM buffer
    std::array<float, 2*WFM_SIZE>& read_mmapbuf_nocopy() {
        auto p = reinterpret_cast<std::array<float, 2*WFM_SIZE>*>(mmap_buf);
        return *p;
    }

    // Read data in RAM buffer
    std::array<float, 2*WFM_SIZE>& read_rambuf_mmap_memcpy() {
        memcpy(mmap_buf, rambuf_data, 2*WFM_SIZE*sizeof(float));
        auto p = reinterpret_cast<std::array<float, 2*WFM_SIZE>*>(mmap_buf);
        return *p;
    }

  private:
    Memory<mem::rambuf>& rambuf_map;

    void *mmap_buf;
    uint32_t *raw_data_1 = nullptr;
    uint32_t *raw_data_2 = nullptr;
    float *rambuf_data = nullptr;

    std::array<float, 2*WFM_SIZE> rambuf_copy;
    std::array<float, 2*WFM_SIZE> data_zeros;
};

#endif // __DRIVERS_SPEED_TEST_HPP__
