/// (c) Koheron

#include "oscillo.hpp"
#include <string.h>

Oscillo::Oscillo(Klib::DevMem& dvm_)
: dvm(dvm_)
, data_decim(0)
{
    config_map = dvm.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
    status_map = dvm.AddMemoryMap(STATUS_ADDR, STATUS_RANGE, PROT_READ);
    adc_1_map = dvm.AddMemoryMap(ADC1_ADDR, ADC1_RANGE);
    adc_2_map = dvm.AddMemoryMap(ADC2_ADDR, ADC2_RANGE);
    dac_map = dvm.AddMemoryMap(DAC_ADDR, DAC_RANGE);

    raw_data_1 = dvm.read_buffer<int32_t>(adc_1_map);
    raw_data_2 = dvm.read_buffer<int32_t>(adc_2_map);

    set_averaging(false); // Reset averaging
    set_period(WFM_SIZE);
    set_n_avg_min(0);
}

void Oscillo::set_period(uint32_t period)
{
    dvm.write32(config_map, PERIOD0_OFF, period - 1);
    dvm.write32(config_map, PERIOD1_OFF, period - 1);
    dvm.write32(config_map, THRESHOLD0_OFF, period - 6);
    dvm.write32(config_map, THRESHOLD1_OFF, period - 6);
}

void Oscillo::set_n_avg_min(uint32_t n_avg_min) 
{
    uint32_t n_avg_min_ = (n_avg_min < 2) ? 0 : n_avg_min-2;
    dvm.write32(config_map, N_AVG_MIN0_OFF, n_avg_min_);
    dvm.write32(config_map, N_AVG_MIN1_OFF, n_avg_min_);
}

void Oscillo::_wait_for_acquisition()
{
    do {} while (dvm.read32(status_map, AVG_READY0_OFF) == 0 
                 || dvm.read32(status_map, AVG_READY1_OFF) == 0);
}

// Read only one channel
std::array<float, WFM_SIZE>& Oscillo::read_data(bool channel)
{
    Klib::MemMapID adc_map;
    channel ? adc_map = adc_1_map : adc_map = adc_2_map;
    dvm.set_bit(config_map, ADDR_OFF, 1);
    _wait_for_acquisition();
    uint32_t *raw_data = dvm.read_buff32(adc_map);
    float num_avg;

    if (dvm.read32(status_map, AVG_ON_OUT0_OFF)) {
        if (channel) {
        num_avg = float(dvm.read32(status_map, N_AVG0_OFF));  
        } else {
            num_avg = float(dvm.read32(status_map, N_AVG1_OFF));  
        }
        for(unsigned int i=0; i < WFM_SIZE; i++)
            data[i] = float(raw_data[i]) / num_avg;
    } else {
        for(unsigned int i=0; i < WFM_SIZE; i++)
            data[i] = float(raw_data[i]);
    }
    dvm.clear_bit(config_map, ADDR_OFF, 1);
    return data;
}

// Read the two channels
std::array<float, 2*WFM_SIZE>& Oscillo::read_all_channels()
{
    dvm.set_bit(config_map, ADDR_OFF, 1);
    _wait_for_acquisition();

    if (dvm.read32(status_map, AVG_ON_OUT0_OFF)) {
        float num_avg0 = float(dvm.read32(status_map, N_AVG0_OFF));
        float num_avg1 = float(dvm.read32(status_map, N_AVG1_OFF)); 
        for(unsigned int i=0; i<WFM_SIZE; i++) {
            data_all[i] = float(raw_data_1[i]) / num_avg0;
            data_all[i + WFM_SIZE] = float(raw_data_2[i]) / num_avg1;
        }
    } else {
        for(unsigned int i=0; i<WFM_SIZE; i++) {
            data_all[i] = float(raw_data_1[i]);
            data_all[i + WFM_SIZE] = float(raw_data_2[i]);
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
        float num_avg = float(get_num_average()); 
        for(unsigned int i=0; i<n_pts; i++) {
            data_decim[i] = float(raw_data_1[index_low + decim_factor * i]) / num_avg;
            data_decim[i + n_pts] = float(raw_data_2[index_low + decim_factor * i]) / num_avg;
        }
    } else {
        for(unsigned int i=0; i<n_pts; i++) {
            data_decim[i] = float(raw_data_1[index_low + decim_factor * i]);
            data_decim[i + n_pts] = float(raw_data_2[index_low + decim_factor * i]);
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
