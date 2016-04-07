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
#define ACQ_TIME_US uint32_t(2*(WFM_SIZE*1E6)/SAMPLING_RATE)

class Oscillo
{
  public:
    Oscillo(Klib::DevMem& dev_mem_);

    int Open();

    std::array<float, WFM_SIZE>& read_data(bool channel);

    std::array<float, 2*WFM_SIZE>& read_all_channels();

    std::vector<float>& read_all_channels_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high);

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
    bool avg_on; // True if averaging is enabled

    uint32_t *raw_data_1 = nullptr;
    uint32_t *raw_data_2 = nullptr;

    // Memory maps IDs:
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID adc_1_map;
    Klib::MemMapID adc_2_map;

    const std::array<Klib::MemoryRegion, 4> mem_regions = {{
        { CONFIG_ADDR, CONFIG_RANGE },
        { STATUS_ADDR, STATUS_RANGE },
        { ADC1_ADDR  , ADC1_RANGE   },
        { ADC2_ADDR  , ADC2_RANGE   }
    }};
    
    // Acquired data buffers
    std::array<float, WFM_SIZE> data;
    std::array<float, 2*WFM_SIZE> data_all;
    std::vector<float> data_decim;
    
    // Internal functions
    void _wait_for_acquisition();
}; // class Oscillo

#endif // __DRIVERS_CORE_OSCILLO_HPP__
