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

constexpr uint32_t dac_sel_width  = ceil(log(float(N_DAC_BRAM_PARAM)) / log(2.));
constexpr uint32_t bram_sel_width = ceil(log(float(N_DAC_PARAM)) / log(2.));

class Oscillo
{
  public:
    Oscillo(Klib::DevMem& dvm_);

    // Reset ...

    void reset() {
        dvm.clear_bit(config_map, ADDR_OFF, 0);
        dvm.set_bit(config_map, ADDR_OFF, 0);
    }

    void reset_acquisition() {
        dvm.clear_bit(config_map, ADDR_OFF, 1);
        dvm.set_bit(config_map, ADDR_OFF, 1);
    }

    void set_averaging(bool avg_on);

    // Monitoring

    uint64_t get_counter() {
        uint64_t lsb = dvm.read32(status_map, COUNTER0_OFF);
        uint64_t msb = dvm.read32(status_map, COUNTER1_OFF);
        return lsb + (msb << 32);
    }

    uint32_t get_num_average()   {return dvm.read32(status_map, N_AVG0_OFF);}
    uint32_t get_num_average_0() {return dvm.read32(status_map, N_AVG0_OFF);}
    uint32_t get_num_average_1() {return dvm.read32(status_map, N_AVG1_OFF);}

    // TODO should be a one-liner
    void set_clken_mask(bool clken_mask) {
        if (clken_mask) {
            dvm.set_bit(config_map, CLKEN_MASK_OFF, 0);
        } else {
            dvm.clear_bit(config_map, CLKEN_MASK_OFF, 0);
        }
    }
    
    void update_now() {
        dvm.set_bit(config_map, CLKEN_MASK_OFF, 1);
        dvm.clear_bit(config_map, CLKEN_MASK_OFF, 1);
    }

    void always_update() {
        dvm.set_bit(config_map, CLKEN_MASK_OFF, 1);
    }
    
    void set_n_avg_min(uint32_t n_avg_min);

    void set_addr_select(uint32_t addr_select) {
        dvm.write32(config_map, ADDR_SELECT_OFF, addr_select);
    }

    void set_dac_periods(uint32_t dac_period0, uint32_t dac_period1) {
        dvm.write32(config_map, DAC_PERIOD0_OFF, dac_period0 - 1);
        dvm.write32(config_map, DAC_PERIOD1_OFF, dac_period1 - 1);
        reset();
    }
    
    void set_avg_period(uint32_t avg_period) {
        dvm.write32(config_map, AVG_PERIOD0_OFF, avg_period - 1);
        dvm.write32(config_map, AVG_THRESHOLD0_OFF, avg_period - 6);
        reset();
    }

    // Write DACs

    void set_dac_buffer(uint32_t channel, const std::array<uint32_t, WFM_SIZE/2>& arr) {
        uint32_t old_idx = bram_index[channel];
        uint32_t new_idx = get_first_empty_bram_index();
        // Write data in empty BRAM
        dvm.write_buff32(dac_map[new_idx], 0, arr.data(), arr.size());
        // Switch DAC interconnect
        bram_index[channel] = new_idx;
        connected_bram[new_idx] = true;
        update_dac_routing();
        connected_bram[old_idx] = false;
    }

    std::array<uint32_t, WFM_SIZE/2>& get_dac_buffer(uint32_t channel)
    {
        uint32_t *buff = dvm.read_buff32(dac_map[bram_index[channel]]);
        auto p = reinterpret_cast<std::array<uint32_t, WFM_SIZE/2>*>(buff);
        assert(p->data() == (const uint32_t*)buff);
        return *p;
    }

    // Read ADC
    std::array<float, 2*WFM_SIZE>& read_all_channels();
    std::vector<float>& read_all_channels_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high);

    // Internal functions
    void _wait_for_acquisition();

    void init_dac_brams() {
        // Use BRAM0 for DAC0, BRAM1 for DAC1 ...
        for (int i=0; i < N_DAC_PARAM; i++) {
            bram_index[i] = i;
            connected_bram[i] = true;
        }
        for (int i=N_DAC_PARAM; i < N_DAC_BRAM_PARAM; i++) {
            connected_bram[i] = false;
        }
        update_dac_routing();
    }

    uint32_t get_first_empty_bram_index() {
        uint32_t i;
        for (i=0; i < N_DAC_BRAM_PARAM; i++) {
            if ((bram_index[0] != i) && (bram_index[1] != i))
                break;
        }
        return i;
    }

    void update_dac_routing() {
        // dac_select defines the connection between BRAMs and DACs
        uint32_t dac_select = 0;
        for (uint32_t i=0; i < N_DAC_PARAM; i++)
            dac_select += bram_index[i] << (dac_sel_width * i);
        dvm.write32(config_map, DAC_SELECT_OFF, dac_select);

        // addr_select defines the connection between address generators and BRAMs
        uint32_t addr_select = 0;
        for (uint32_t j=0; j < N_DAC_PARAM; j++)
            addr_select += j << (bram_sel_width * bram_index[j]);
        dvm.write32(config_map, ADDR_SELECT_OFF, addr_select);
    }

  private:
    Klib::DevMem& dvm;

    int32_t *raw_data[2] = {nullptr, nullptr};

    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID adc_map[2];
    Klib::MemMapID dac_map[N_DAC_BRAM_PARAM];
    
    // Acquired data buffers
    std::array<float, 2*WFM_SIZE> data_all;
    std::vector<float> data_decim;

    // Store the BRAM corresponding to each DAC
    std::array<uint32_t, N_DAC_PARAM> bram_index;
    std::array<bool, N_DAC_BRAM_PARAM> connected_bram;

}; // class Oscillo

#endif // __DRIVERS_CORE_OSCILLO_HPP__
