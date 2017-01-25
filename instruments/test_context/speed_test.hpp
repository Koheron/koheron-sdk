/// SpeedTest driver
///
/// (c) Koheron

#ifndef __DRIVERS_SPEED_TEST_HPP__
#define __DRIVERS_SPEED_TEST_HPP__

#include <vector>
#include <cstring>

#include <context.hpp>

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
        memset(mmap_buf, 0, 16384*4);
        data_zeros.fill(0);
    }

    // Return zeros
    const auto& read_zeros() {
        return data_zeros;
    }

    // Read data in RAM buffer
    const auto& read_rambuf() {
        return rambuf_map.read_array<float, buff_size>();
    }

    // Read data in RAM buffer (with copy)
    const auto& read_rambuf_memcpy() {
        memcpy((unsigned char*)rambuf_copy.data(), (unsigned char*)rambuf_data, buff_size * sizeof(float));
        return rambuf_copy;
    }

    // Read data in RAM buffer (with copy)
    const auto& read_rambuf_std_copy() {
        std::copy(rambuf_data, rambuf_data + buff_size, rambuf_copy.begin());
        return rambuf_copy;
    }

    // Read data in RAM buffer (with optimized copy)
    const auto& read_rambuf_mycopy() {
        mycopy((unsigned char*)rambuf_copy.data(), (unsigned char*)rambuf_data, buff_size * sizeof(float));
        return rambuf_copy;
    }

    // Read data in RAM buffer
    const auto& read_mmapbuf_nocopy() {
        const auto p = reinterpret_cast<const std::array<float, buff_size>*>(mmap_buf);
        return *p;
    }

    // Read data in RAM buffer
    const auto& read_rambuf_mmap_memcpy() {
        memcpy(mmap_buf, rambuf_data, buff_size * sizeof(float));
        const auto p = reinterpret_cast<const std::array<float, buff_size>*>(mmap_buf);
        return *p;
    }

  private:
    static constexpr size_t buff_size = 64 * 1024 / sizeof(float);

    Memory<mem::rambuf>& rambuf_map;

    void *mmap_buf;
    uint32_t *raw_data_1 = nullptr;
    uint32_t *raw_data_2 = nullptr;
    float *rambuf_data = nullptr;

    std::array<float, buff_size> rambuf_copy;
    std::array<float, buff_size> data_zeros;
};

#endif // __DRIVERS_SPEED_TEST_HPP__
