/// Oscilloscope driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_OSCILLO_HPP__
#define __DRIVERS_CORE_OSCILLO_HPP__

#include <vector>

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>
#include <drivers/addresses.hpp>

#include <signal/kvector.hpp>
 
#define SAMPLING_RATE 125E6

class Oscillo
{
  public:
    Oscillo(Klib::DevMem& dev_mem_);
    ~Oscillo();

    int Open(uint32_t waveform_size_);

    #pragma tcp-server exclude
    void Close();

    std::vector<float>& read_data(bool channel);
    std::vector<float>& read_all_channels();

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
    
    // Internal functions
    void _wait_for_acquisition();
    void _raw_to_vector(uint32_t *raw_data);
    void _raw_to_vector_all(uint32_t *raw_data_1, uint32_t *raw_data_2);
}; // class Oscillo

#endif // __DRIVERS_CORE_OSCILLO_HPP__
