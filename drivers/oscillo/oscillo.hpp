/// Oscilloscope driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_OSCILLO_HPP__
#define __DRIVERS_CORE_OSCILLO_HPP__

#include <vector>
#include <algorithm>
#include <math.h>

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/dac_router.hpp>
#include <drivers/addresses.hpp>

#define SAMPLING_RATE 125E6
#define WFM_SIZE ADC1_RANGE/sizeof(float)
#define ACQ_PERIOD_NS 8 // Duration between two acquisitions (ns)

constexpr uint32_t wfm_time_ns = WFM_SIZE * static_cast<uint32_t>(1E9 / SAMPLING_RATE);
constexpr std::array<uint32_t, 2> n_avg_offset = {N_AVG0_OFF, N_AVG1_OFF};

constexpr std::array<std::array<uint32_t, 2>, N_DAC_BRAM_PARAM> dac_brams  = {{
    {DAC1_ADDR, DAC1_RANGE},
    {DAC2_ADDR, DAC2_RANGE},
    {DAC3_ADDR, DAC3_RANGE}
}};

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

    uint32_t get_num_average(uint32_t channel)  {return dvm.read32(status_map, n_avg_offset[channel]);}

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
    
    void set_n_avg_min(uint32_t n_avg_min) {
        n_avg_min_ = (n_avg_min < 2) ? 0 : n_avg_min-2;
        dvm.write32(config_map, N_AVG_MIN0_OFF, n_avg_min_);
        dvm.write32(config_map, N_AVG_MIN1_OFF, n_avg_min_);
    }

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

    // DACs
    void set_dac_buffer(uint32_t channel, const std::array<uint32_t, WFM_SIZE/2>& arr);
    std::array<uint32_t, WFM_SIZE/2>& get_dac_buffer(uint32_t channel);

    // Read ADC
    std::array<float, 2*WFM_SIZE>& read_all_channels();
    std::vector<float>& read_all_channels_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high);

  private:
    Klib::DevMem& dvm;

    int32_t *raw_data[2] = {nullptr, nullptr};

    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID adc_map[2];
    
    // Acquired data buffers
    std::array<float, 2*WFM_SIZE> data_all;
    std::vector<float> data_decim;

    // Store the BRAM corresponding to each DAC
    Klib::DacRouter<N_DAC_PARAM, N_DAC_BRAM_PARAM> dac_router;

    uint32_t n_avg_min_;

    // Internal functions
    void _wait_for_acquisition();
}; // class Oscillo

#endif // __DRIVERS_CORE_OSCILLO_HPP__
