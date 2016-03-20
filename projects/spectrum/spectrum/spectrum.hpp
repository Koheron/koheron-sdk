/// Spectrum analyzer driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_SPECTRUM_HPP__
#define __DRIVERS_CORE_SPECTRUM_HPP__

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>
#include <drivers/addresses.hpp>

#define SAMPLING_RATE 125E6
#define WFM_SIZE SPECTRUM_RANGE/sizeof(float)
#define ACQ_TIME_US uint32_t(2*(WFM_SIZE*1E6)/SAMPLING_RATE)

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf
#define PEAK_FIFO_ADDR 0x43C10000
#define PEAK_FIFO_RANGE 64*1024
#define PEAK_RDFR_OFF 0x18
#define PEAK_RDFO_OFF 0x1C
#define PEAK_RDFD_OFF 0x20
#define PEAK_RLR_OFF 0x24

class Spectrum
{
  public:
    Spectrum(Klib::DevMem& dev_mem_);
    ~Spectrum();

    int Open();
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

    void set_address_range(uint32_t address_low, uint32_t address_high);

    uint32_t get_peak_fifo_occupancy();
    uint32_t get_peak_fifo_length();
    void reset_peak_fifo();
    std::vector<uint32_t>& get_peak_fifo_data(uint32_t n_pts);

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

    bool avg_on; // True if averaging is enabled

    void Close();

    // Memory maps IDs:
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID spectrum_map;
    Klib::MemMapID demod_map;
    Klib::MemMapID noise_floor_map;
    Klib::MemMapID peak_fifo_map;

    // Acquired data buffers
    float *raw_data;
    std::array<float, WFM_SIZE> spectrum_data;
    std::vector<float> spectrum_decim;
    std::vector<uint32_t> peak_fifo_data;
    
    // Internal functions
    void _wait_for_acquisition();
}; // class Spectrum

#endif // __DRIVERS_CORE_SPECTRUM_HPP__
