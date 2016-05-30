/// Spectrum analyzer driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_SPECTRUM_HPP__
#define __DRIVERS_CORE_SPECTRUM_HPP__

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/wr_register.hpp>
#include <drivers/lib/fifo_reader.hpp>

#include <drivers/addresses.hpp>

#define SAMPLING_RATE 125E6
#define WFM_SIZE SPECTRUM_RANGE/sizeof(float)
#define FIFO_BUFF_SIZE 4096

class Spectrum
{
  public:
    Spectrum(Klib::DevMem& dvm_);

    int Open();

    void set_period(uint32_t period);
    void set_n_avg_min(uint32_t n_avg_min);

    void reset();

    #pragma tcp-server write_array arg{data} arg{len}
    void set_dac_buffer(const uint32_t *data, uint32_t len);

    void reset_acquisition();

    void set_scale_sch(uint32_t scale_sch);
    void set_offset(uint32_t offset_real, uint32_t offset_imag);

    #pragma tcp-server write_array arg{data} arg{len}
    void set_demod_buffer(const uint32_t *data, uint32_t len);

    #pragma tcp-server write_array arg{data} arg{len}
    void set_noise_floor_buffer(const uint32_t *data, uint32_t len);

    std::array<float, WFM_SIZE>& get_spectrum();

    std::vector<float>& get_spectrum_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high);

    void set_averaging(bool avg_status);

    uint32_t get_num_average();
    uint32_t get_peak_address();
    uint32_t get_peak_maximum();

    /// @acq_period Sleeping time between two acquisitions (us)
    void fifo_start_acquisition(uint32_t acq_period);
    void fifo_stop_acquisition();
    void set_address_range(uint32_t address_low, uint32_t address_high);
    uint32_t get_peak_fifo_length();
    uint32_t store_peak_fifo_data();
    std::vector<uint32_t>& get_peak_fifo_data();
    bool fifo_get_acquire_status();

    enum Status {
        CLOSED,
        OPENED,
        FAILED
    };

    #pragma tcp-server is_failed
    bool IsFailed() const {return status == FAILED;}

  private:
    Klib::DevMem& dvm;
    int status;

    // Memory maps IDs:
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID spectrum_map;
    Klib::MemMapID demod_map;
    Klib::MemMapID noise_floor_map;
    Klib::MemMapID peak_fifo_map;
    Klib::MemMapID dac_map;

    // Acquired data buffers
    float *raw_data;
    std::array<float, WFM_SIZE> spectrum_data;
    std::vector<float> spectrum_decim;
    FIFOReader<FIFO_BUFF_SIZE> fifo;
    
    // Internal functions
    void _wait_for_acquisition();
}; // class Spectrum

#endif // __DRIVERS_CORE_SPECTRUM_HPP__
