/// Oscilloscope driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_OSCILLO_HPP__
#define __DRIVERS_CORE_OSCILLO_HPP__

#include <vector>

#include <drivers/lib/dev_mem.hpp>
#include <drivers/addresses.hpp>

#define SAMPLING_RATE 125E6
#define WFM_SIZE ADC1_RANGE/sizeof(float)

class Oscillo
{
  public:
    Oscillo(Klib::DevMem& dvm_);

    int Open() {return dvm.is_ok() ? 0 : -1;}

    void reset() {
        dvm.clear_bit(config_map, ADDR_OFF, 1);
        dvm.set_bit(config_map, ADDR_OFF, 0);
    }

    void set_n_avg_min(uint32_t n_avg_min);
    void set_period(uint32_t period);

    #pragma tcp-server write_array arg{data} arg{len}
    void set_dac_buffer(const uint32_t *data, uint32_t len, uint32_t channel) {
        if (channel == 1) {
            dvm.write_buff32(dac_1_map, 0, data, len);
        } else if (channel == 2) {
            dvm.write_buff32(dac_2_map, 0, data, len);
        }
    }

    void reset_acquisition() {
        dvm.write32(config_map, ADDR_OFF, 1);
        dvm.write32(config_map, ADDR_OFF, 1);
    }

    std::array<float, WFM_SIZE>& read_data(bool channel);

    std::array<float, 2*WFM_SIZE>& read_all_channels();

    std::vector<float>& read_all_channels_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high);

    void set_averaging(bool avg_on);
    
    uint32_t get_num_average() {return dvm.read32(status_map, N_AVG0_OFF);}

    #pragma tcp-server is_failed
    bool IsFailed() const {return dvm.IsFailed();}

  private:
    Klib::DevMem& dvm;

    int32_t *raw_data_1 = nullptr;
    int32_t *raw_data_2 = nullptr;

    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID adc_1_map;
    Klib::MemMapID adc_2_map;
    Klib::MemMapID dac_1_map;
    Klib::MemMapID dac_2_map;
    
    // Acquired data buffers
    std::array<float, WFM_SIZE> data;
    std::array<float, 2*WFM_SIZE> data_all;
    std::vector<float> data_decim;
    
    // Internal functions
    void _wait_for_acquisition();
}; // class Oscillo

#endif // __DRIVERS_CORE_OSCILLO_HPP__
