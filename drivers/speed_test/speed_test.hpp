/// SpeedTestscope driver
///
/// (c) Koheron

#ifndef __DRIVERS_SPEED_TEST_HPP__
#define __DRIVERS_SPEED_TEST_HPP__

#include <vector>
#include <cstring>

#include <drivers/lib/memory_manager.hpp>
#include <drivers/memory.hpp>

#define SAMPLING_RATE 125E6

#define WFM_SIZE mem::adc_range/sizeof(float)

// #define RAMBUF_ADDR 0x1E000000
// #define RAMBUF_RANGE 2048*4096

//http://es.codeover.org/questions/34888683/arm-neon-memcpy-optimized-for-uncached-memory
void mycopy(volatile unsigned char *dst, volatile unsigned char *src, int sz);

class SpeedTest
{
  public:
    SpeedTest(MemoryManager& mm);

    std::array<float, 2*WFM_SIZE>& read_raw_all();

    // Return zeros (does not perform FPGA memory access)
    std::array<float, 2*WFM_SIZE>& read_zeros() {return data_zeros;}

    // Read data in RAM buffer
    #pragma tcp-server read_array 2*WFM_SIZE
    float* read_rambuf() {return rambuf_data;}

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
    #pragma tcp-server read_array 2*WFM_SIZE
    float* read_mmapbuf_nocopy() {return (float*)mmap_buf;}

    // Read data in RAM buffer
    #pragma tcp-server read_array 2*WFM_SIZE
    float* read_rambuf_mmap_memcpy() {
        memcpy(mmap_buf, rambuf_data, 2*WFM_SIZE*sizeof(float));
        return (float*)mmap_buf;
    }

  private:
    MemoryMap<mem::config>& cfg;
    MemoryMap<mem::status>& sts;
    MemoryMap<mem::adc>& adc_map;
    MemoryMap<mem::rambuf>& rambuf_map;

    void *mmap_buf;
    uint32_t *raw_data_1 = nullptr;
    uint32_t *raw_data_2 = nullptr;
    float *rambuf_data = nullptr;
    std::array<float, 2*WFM_SIZE> rambuf_copy;

    // Acquired data buffers
    std::array<float, 2*WFM_SIZE> data_all;
    std::array<float, 2*WFM_SIZE> data_zeros;
    std::vector<float> data_decim;
    std::vector<uint32_t> data_all_int;
}; // class SpeedTest

#endif // __DRIVERS_SPEED_TEST_HPP__
