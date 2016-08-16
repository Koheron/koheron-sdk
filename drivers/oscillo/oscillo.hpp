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
#define WFM_SIZE ADC_RANGE/sizeof(float)
#define ACQ_PERIOD_NS 8 // Duration between two acquisitions (ns)

constexpr uint32_t wfm_time_ns = WFM_SIZE * static_cast<uint32_t>(1E9 / SAMPLING_RATE);
constexpr std::array<uint32_t, 2> n_avg_offset = {N_AVG0_OFF, N_AVG1_OFF};

class Oscillo
{
  public:
    Oscillo(DevMem& dvm_);

    // Reset ...

    void reset() {
        cfg.clear_bit<ADDR_OFF, 0>();
        cfg.set_bit<ADDR_OFF, 0>();
    }

    void reset_acquisition() {
        cfg.clear_bit<ADDR_OFF, 1>();
        cfg.set_bit<ADDR_OFF, 1>();
    }

    void set_averaging(bool avg_on);

    // Monitoring

    uint64_t get_counter() {
        uint64_t lsb = sts.read<COUNTER0_OFF>();
        uint64_t msb = sts.read<COUNTER1_OFF>();
        return lsb + (msb << 32);
    }

    uint32_t get_num_average(uint32_t channel) {
        return sts.read_offset(n_avg_offset[channel]);
    }

    // TODO should be a one-liner
    void set_clken_mask(bool clken_mask) {
        if (clken_mask) {
            cfg.set_bit<CLKEN_MASK_OFF, 0>();
        } else {
            cfg.clear_bit<CLKEN_MASK_OFF, 0>();
        }
    }

    void update_now() {
        cfg.set_bit<CLKEN_MASK_OFF, 1>();
        cfg.clear_bit<CLKEN_MASK_OFF, 1>();
    }

    void always_update() {
        cfg.set_bit<CLKEN_MASK_OFF, 1>();
    }

    void set_n_avg_min(uint32_t n_avg_min) {
        n_avg_min_ = (n_avg_min < 2) ? 0 : n_avg_min-2;
        cfg.write<N_AVG_MIN0_OFF>(n_avg_min_);
        cfg.write<N_AVG_MIN1_OFF>(n_avg_min_);
    }

    void set_addr_select(uint32_t addr_select) {
        cfg.write<ADDR_SELECT_OFF>(addr_select);
    }

    void set_dac_periods(uint32_t dac_period0, uint32_t dac_period1) {
        cfg.write<DAC_PERIOD0_OFF>(dac_period0 - 1);
        cfg.write<DAC_PERIOD1_OFF>(dac_period1 - 1);
        reset();
    }

    void set_avg_period(uint32_t avg_period) {
        cfg.write<AVG_PERIOD_OFF>(avg_period - 1);
        cfg.write<AVG_THRESHOLD_OFF>(avg_period - 6);
        reset();
    }

    // DACs
    void set_dac_buffer(uint32_t channel, const std::array<uint32_t, WFM_SIZE/2>& arr) {
        dac.set_data(channel, arr);
    }

    void set_dac_float(uint32_t channel, const std::array<float, WFM_SIZE>& arr) {
        dac.set_data<DAC_WIDTH_PARAM>(channel, arr);
    }

    std::array<uint32_t, WFM_SIZE/2>& get_dac_buffer(uint32_t channel) {
        return dac.get_data<WFM_SIZE/2>(channel);
    }

    // Read ADC
    std::array<float, 2*WFM_SIZE>& read_all_channels();
    std::vector<float>& read_all_channels_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high);

  private:
    DevMem& dvm;

    int32_t *raw_data[2] = {nullptr, nullptr};

    MemoryMap<CONFIG_ID>& cfg;
    MemoryMap<STATUS_ID>& sts;
    MemoryMap<ADC_ID> adc_map;

    // Acquired data buffers
    std::array<float, 2*WFM_SIZE> data_all;
    std::vector<float> data_decim;

    DacRouter<N_DAC_PARAM, N_DAC_BRAM_PARAM> dac;

    uint32_t n_avg_min_;

    // Internal functions
    void _wait_for_acquisition();
}; // class Oscillo

#endif // __DRIVERS_CORE_OSCILLO_HPP__
