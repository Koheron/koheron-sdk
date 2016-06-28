/// Oscilloscope driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_OSCILLO_HPP__
#define __DRIVERS_CORE_OSCILLO_HPP__

#include <vector>    
#include <algorithm>
#include <thread>
#include <chrono>

#include <drivers/lib/dev_mem.hpp>
#include <math.h>
#include <drivers/addresses.hpp>

#define SAMPLING_RATE 125E6
#define WFM_SIZE ADC1_RANGE/sizeof(float)

constexpr uint32_t sel_width = ceil(log(float(N_DAC_BRAM_PARAM)) / log(2.));

class Oscillo
{
  public:
    Oscillo(Klib::DevMem& dvm_);

    int Open() {return dvm.is_ok() ? 0 : -1;}

    void reset() {
        dvm.clear_bit(config_map, ADDR_OFF, 0);
        dvm.set_bit(config_map, ADDR_OFF, 0);
    }

    void set_n_avg_min(uint32_t n_avg_min);

    void set_period(uint32_t period) {
        set_dac_period(period, period);
        set_avg_period(period, period);
        reset();
    }

    void set_dac_period(uint32_t dac_period0, uint32_t dac_period1) {
        dvm.write32(config_map, DAC_PERIOD0_OFF, dac_period0 - 1);
        dvm.write32(config_map, DAC_PERIOD1_OFF, dac_period1 - 1);
    }
    
    void set_avg_period(uint32_t avg_period0, uint32_t avg_period1) {
        dvm.write32(config_map, AVG_PERIOD0_OFF, avg_period0 - 1);
        dvm.write32(config_map, AVG_PERIOD1_OFF, avg_period1 - 1);
        dvm.write32(config_map, AVG_THRESHOLD0_OFF, avg_period0 - 6);
        dvm.write32(config_map, AVG_THRESHOLD1_OFF, avg_period1 - 6);
    }

    void init_dac_brams() {
        for (int i=0; i < N_DAC_PARAM; i++) {
            bram_index[i] = i;
            connected_bram[i] = true;
        }
        for (int i=N_DAC_PARAM; i < N_DAC_BRAM_PARAM; i++) {
            connected_bram[i] = false;
        }
        set_dac_select();
    }

    void set_dac_select() {
        uint32_t dac_select = 0;
        for (uint32_t i=0; i < N_DAC_PARAM; i++) {
            dac_select += bram_index[i] << (sel_width * i);
        }
        dvm.write32(config_map, DAC_SELECT_OFF, dac_select);
    }

    uint32_t get_first_empty_bram_index() {
        for (uint32_t i=0; i < N_DAC_BRAM_PARAM; i++) {
            if (connected_bram[i] == false)
                return i;
        }
        return N_DAC_BRAM_PARAM;
    }

    #pragma tcp-server write_array arg{data} arg{len}
    void set_dac_buffer(uint32_t channel, const uint32_t *data, uint32_t len) {
        uint32_t old_idx = bram_index[channel];
        uint32_t new_idx = get_first_empty_bram_index();
        // Write data in empty BRAM
        dvm.write_buff32(dac_map[new_idx], 0, data, len);
        // Switch interconnect
        bram_index[channel] = new_idx;
        connected_bram[new_idx] = true;
        set_dac_select();
        connected_bram[old_idx] = false;
    }

    #pragma tcp-server write_array arg{data} arg{len}
    void set_dac_buffer_no_switch(uint32_t channel, const uint32_t *data, uint32_t len) {
        uint32_t idx = bram_index[channel];
        dvm.write_buff32(dac_map[idx], 0, data, len);
    }

    void reset_acquisition() {
        dvm.clear_bit(config_map, ADDR_OFF, 1);
        dvm.set_bit(config_map, ADDR_OFF, 1);
    }

    std::array<uint32_t, N_DAC_PARAM>& get_bram_index() {return bram_index;}

    std::array<float, 2*WFM_SIZE>& read_all_channels();

    std::vector<float>& read_all_channels_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high);

    void set_averaging(bool avg_on);
    
    uint32_t get_num_average()   {return dvm.read32(status_map, N_AVG0_OFF);}
    uint32_t get_num_average_0() {return dvm.read32(status_map, N_AVG0_OFF);}
    uint32_t get_num_average_1() {return dvm.read32(status_map, N_AVG1_OFF);}

    uint64_t get_counter() {
        uint64_t lsb = dvm.read32(status_map, COUNTER0_OFF);
        uint64_t msb = dvm.read32(status_map, COUNTER1_OFF);
        return lsb + (msb << 32);
    }

    #pragma tcp-server is_failed
    bool IsFailed() const {return dvm.IsFailed();}

  private:
    Klib::DevMem& dvm;

    int32_t *raw_data[2] = {nullptr, nullptr};

    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID adc_map[2];
    Klib::MemMapID dac_map[N_DAC_BRAM_PARAM];
    
    // Acquired data buffers
    std::array<float, WFM_SIZE> data;
    std::array<float, 2*WFM_SIZE> data_all;
    std::vector<float> data_decim;

    // Store the BRAM corresponding to each DAC
    std::array<uint32_t, N_DAC_PARAM> bram_index;
    std::array<bool, N_DAC_BRAM_PARAM> connected_bram;

    // Internal functions
    void _wait_for_acquisition();
}; // class Oscillo

#endif // __DRIVERS_CORE_OSCILLO_HPP__
