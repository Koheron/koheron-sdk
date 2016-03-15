/// Oscilloscope driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_OSCILLO_HPP__
#define __DRIVERS_CORE_OSCILLO_HPP__

#include <vector>

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>
#include <drivers/addresses.hpp>

#define SAMPLING_RATE 125E6

#define WFM_SIZE ADC1_RANGE/sizeof(float)

#define RAMBUF_ADDR 0x1E000000
#define RAMBUF_RANGE 2048*4096

//http://es.codeover.org/questions/34888683/arm-neon-memcpy-optimized-for-uncached-memory
void mycopy(volatile unsigned char *dst, volatile unsigned char *src, int sz);

class Oscillo
{
  public:
    Oscillo(Klib::DevMem& dev_mem_);
    ~Oscillo();

    int Open(uint32_t waveform_size_);

    std::array<float, WFM_SIZE>& read_data(bool channel);

    std::array<float, 2*WFM_SIZE>& read_all_channels();

    std::vector<float>& read_all_channels_decim(uint32_t decim_factor);

    std::array<float, 2*WFM_SIZE>& read_raw_all();

    std::array<float, 2*WFM_SIZE>& read_zeros();

    #pragma tcp-server read_array 2*WFM_SIZE
    float* read_rambuf();

    std::array<float, 2*WFM_SIZE>& read_rambuf_memcpy();

    std::array<float, 2*WFM_SIZE>& read_rambuf_mycopy();

    #pragma tcp-server read_array 2*WFM_SIZE
    float* read_mmapbuf_nocopy();

    #pragma tcp-server read_array 2*WFM_SIZE
    float* read_rambuf_mmap_memcpy();

    std::vector<uint32_t> speed_test(uint32_t n_outer_loop, uint32_t n_inner_loop, uint32_t n_pts);

    void set_averaging(bool avg_status);
    
    uint32_t get_num_average();

    enum Status {
        CLOSED,
        OPENED,
        FAILED
    };

    #pragma tcp-server is_failed
    bool IsFailed() const {return status == FAILED;}

  private:
    Klib::DevMem& dev_mem;

    void Close();

    int status;
    bool avg_on; ///< True if averaging is enabled
    uint32_t waveform_size;
    uint32_t acq_time_us;

    uint32_t *raw_data_1 = nullptr;
    uint32_t *raw_data_2 = nullptr;
    float *rambuf_data = nullptr;
    std::array<float, 2*WFM_SIZE> rambuf_copy;

    // Memory maps IDs:
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID adc_1_map;
    Klib::MemMapID adc_2_map;
    Klib::MemMapID rambuf_map;
    void *mmap_buf;
    
    // Acquired data buffers
    std::array<float, WFM_SIZE> data;
    std::array<float, 2*WFM_SIZE> data_all;
    std::array<float, 2*WFM_SIZE> data_zeros;
    std::vector<float> data_decim;
    std::vector<uint32_t> data_all_int;
    
    // Internal functions
    void _wait_for_acquisition();
}; // class Oscillo

#endif // __DRIVERS_CORE_OSCILLO_HPP__
