/// (c) Koheron

#include "oscillo.hpp"

#include <string.h>
#include <thread>
#include <chrono>

Oscillo::Oscillo(DevMem& dvm_)
: dvm(dvm_)
, data_decim(0)
, dac(dvm_, dac_brams)
{
    config_map = dvm.add_memory_map(CONFIG_ADDR, CONFIG_RANGE);
    status_map = dvm.add_memory_map(STATUS_ADDR, STATUS_RANGE, PROT_READ);

    adc_map[0] = dvm.add_memory_map(ADC1_ADDR, ADC1_RANGE);
    adc_map[1] = dvm.add_memory_map(ADC2_ADDR, ADC2_RANGE);

    raw_data[0] = dvm.get_buffer_ptr<int32_t>(adc_map[0]);
    raw_data[1] = dvm.get_buffer_ptr<int32_t>(adc_map[1]);

    set_averaging(false); // Reset averaging

    // set tvalid delay to 19 * 8 ns
    dvm.write32(config_map, ADDR_OFF, 19 << 2);
    set_n_avg_min(0);
    set_clken_mask(true);
    set_dac_periods(WFM_SIZE, WFM_SIZE);
    set_avg_period(WFM_SIZE);

    dac.set_config_reg(config_map, DAC_SELECT_OFF, ADDR_SELECT_OFF);
}

// Read the two channels
std::array<float, 2*WFM_SIZE>& Oscillo::read_all_channels()
{
    dvm.set_bit(config_map, ADDR_OFF, 1);
    _wait_for_acquisition();

    if (dvm.read32(status_map, AVG_ON_OUT0_OFF)) {
        float num_avg0 = float(get_num_average(0));
        float num_avg1 = float(get_num_average(1));
        for (unsigned int i=0; i<WFM_SIZE; i++) {
            data_all[i] = float(raw_data[0][i]) / num_avg0;
            data_all[i + WFM_SIZE] = float(raw_data[1][i]) / num_avg1;
        }
    } else {
        for (unsigned int i=0; i<WFM_SIZE; i++) {
            data_all[i] = float(raw_data[0][i]);
            data_all[i + WFM_SIZE] = float(raw_data[1][i]);
        }
    }
    dvm.clear_bit(config_map, ADDR_OFF, 1);
    return data_all;
}

// Read the two channels but take only one point every decim_factor points
std::vector<float>& Oscillo::read_all_channels_decim(uint32_t decim_factor, 
                                                     uint32_t index_low, uint32_t index_high)
{
    // Sanity checks
    if (index_high <= index_low || index_high >= WFM_SIZE) {
        data_decim.resize(0);
        return data_decim;
    }

    dvm.set_bit(config_map, ADDR_OFF, 1);
    uint32_t n_pts = (index_high - index_low)/decim_factor;
    data_decim.resize(2*n_pts);
    _wait_for_acquisition();

    uint32_t avg_on = bool(dvm.read32(status_map, AVG_ON_OUT0_OFF));
    if (avg_on) {
        float num_avg = float(get_num_average(0)); 
        for (unsigned int i=0; i<n_pts; i++) {
            data_decim[i] = float(raw_data[0][index_low + decim_factor * i]) / num_avg;
            data_decim[i + n_pts] = float(raw_data[1][index_low + decim_factor * i]) / num_avg;
        }
    } else {
        for (unsigned int i=0; i<n_pts; i++) {
            data_decim[i] = float(raw_data[0][index_low + decim_factor * i]);
            data_decim[i + n_pts] = float(raw_data[1][index_low + decim_factor * i]);
        }
    }
    dvm.clear_bit(config_map, ADDR_OFF, 1);
    return data_decim;
}

void Oscillo::set_averaging(bool avg_on)
{
    if (avg_on) {
        dvm.set_bit(config_map, AVG0_OFF, 0);
        dvm.set_bit(config_map, AVG1_OFF, 0);
    } else {
        dvm.clear_bit(config_map, AVG0_OFF, 0);
        dvm.clear_bit(config_map, AVG1_OFF, 0);
    }
}

void Oscillo::_wait_for_acquisition()
{
    auto begin = std::chrono::high_resolution_clock::now();

    do {
        if (n_avg_min_ > 0) {
            auto now = std::chrono::high_resolution_clock::now();
            uint32_t n_avg_min = dvm.read32(config_map, N_AVG_MIN0_OFF);
            uint64_t acq_time_ns = n_avg_min * wfm_time_ns;
            auto remain_wait = acq_time_ns - std::chrono::duration_cast<std::chrono::nanoseconds>(now-begin).count();

            // If acquisition time is larger than 1 ms, we sleep for the
            // typical overhead time to put the thread in sleep (~ 100 us).

            if (remain_wait > 1000000)
                std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<uint32_t>(remain_wait / 10)));
        }
    } while (dvm.read32(status_map, AVG_READY0_OFF) == 0
             || dvm.read32(status_map, AVG_READY1_OFF) == 0);
}
