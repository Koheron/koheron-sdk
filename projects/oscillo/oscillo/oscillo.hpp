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

//> \description Oscilloscope driver
class Oscillo
{
  public:
    Oscillo(Klib::DevMem& dev_mem_);
    ~Oscillo();

    //> \io_type READ
    int Open(uint32_t waveform_size_);
    
    void Close();

    //> \io_type READ
    std::vector<float>& read_data(bool channel);

    //> \io_type READ
    std::vector<float>& read_all_channels();

    //> \io_type READ_ARRAY param => two_n_pts
    float* read_all_channels_decim(uint32_t two_n_pts);

    //> \io_type READ
    std::vector<float>& read_raw_all();

    //> \io_type READ
    std::vector<float>& read_zeros();

    //> \io_type READ
    std::vector<uint32_t> speed_test(uint32_t n_outer_loop, uint32_t n_inner_loop, uint32_t n_pts);

    //> \io_type WRITE
    void set_averaging(bool avg_status);
    
    //> \io_type READ
    uint32_t get_num_average();

    enum Status {
        CLOSED,
        OPENED,
        FAILED
    };

    //> \is_failed
    bool IsFailed() const {return status == FAILED;}

  private:
    Klib::DevMem& dev_mem;

    int status;
    
    bool avg_on; ///< True if averaging is enabled
    
    uint32_t waveform_size;
    uint32_t acq_time_us;

    // Memory maps IDs:
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID adc_1_map;
    Klib::MemMapID adc_2_map;
    
    // Acquired data buffers
    std::vector<float> data;
    std::vector<float> data_all;
    std::vector<float> data_zeros;
    std::vector<uint32_t> data_all_int;
    
    // Internal functions
    void _wait_for_acquisition();
    void _raw_to_vector(uint32_t *raw_data);
    void _raw_to_vector_all(uint32_t *raw_data_1, uint32_t *raw_data_2);
    void _raw_to_vector_all_raw(uint32_t *raw_data_1, uint32_t *raw_data_2);
}; // class Oscillo

#endif // __DRIVERS_CORE_OSCILLO_HPP__
