/// (c) Koheron

#include "oscillo.hpp"

#include <string.h>
#include <thread>
#include <chrono>

Oscillo::Oscillo(Klib::DevMem& dvm_)
: dvm(dvm_)
, data_decim(0)
{
    config_map = dvm.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
    status_map = dvm.AddMemoryMap(STATUS_ADDR, STATUS_RANGE, PROT_READ);

    adc_map[0] = dvm.AddMemoryMap(ADC1_ADDR, ADC1_RANGE);
    adc_map[1] = dvm.AddMemoryMap(ADC2_ADDR, ADC2_RANGE);

    dac_map[0] = dvm.AddMemoryMap(DAC1_ADDR, DAC2_RANGE);
    dac_map[1] = dvm.AddMemoryMap(DAC2_ADDR, DAC2_RANGE);
    dac_map[2] = dvm.AddMemoryMap(DAC3_ADDR, DAC3_RANGE);

    init_dac_brams();
    
    raw_data[0] = dvm.read_buffer<int32_t>(adc_map[0]);
    raw_data[1] = dvm.read_buffer<int32_t>(adc_map[1]);

    set_averaging(false); // Reset averaging

    // set tvalid delay to 19 * 8 ns
    dvm.write32(config_map, ADDR_OFF, 19 << 2);
    set_n_avg_min(0);
    set_clken_mask(true);
    set_dac_periods(WFM_SIZE, WFM_SIZE);
    set_avg_period(WFM_SIZE);
}

void Oscillo::set_dac_buffer(uint32_t channel, const std::array<uint32_t, WFM_SIZE/2>& arr)
{
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

std::array<uint32_t, WFM_SIZE/2>& Oscillo::get_dac_buffer(uint32_t channel)
{
    uint32_t *buff = dvm.read_buff32(dac_map[bram_index[channel]]);
    auto p = reinterpret_cast<std::array<uint32_t, WFM_SIZE/2>*>(buff);
    assert(p->data() == (const uint32_t*)buff);
    return *p;
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
            data_all[i] = float(raw_data[0][i]) / num_avg0;
            data_all[i + WFM_SIZE] = float(raw_data[1][i]) / num_avg1;
        }
    } else {
        for(unsigned int i=0; i<WFM_SIZE; i++) {
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
        float num_avg = float(get_num_average()); 
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
    avg_on_ = avg_on;

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
    uint64_t acq_time_ns = n_avg_min_ * WFM_SIZE * ACQ_PERIOD_NS;
    auto begin = std::chrono::high_resolution_clock::now();

    do {
        auto now = std::chrono::high_resolution_clock::now();
        auto remain_wait = acq_time_ns - std::chrono::duration_cast<std::chrono::nanoseconds>(now-begin).count();

        // If acquisition time is larger than 1 ms, we sleep for the
        // typical overhead time to put the thread in sleep (~ 100 us).

        if (remain_wait > 1000000)
            std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<uint32_t>(remain_wait / 10)));
    } while (dvm.read32(status_map, AVG_READY0_OFF) == 0
             || dvm.read32(status_map, AVG_READY1_OFF) == 0);
}

void Oscillo::init_dac_brams() {
    // Use BRAM0 for DAC0, BRAM1 for DAC1 ...
    for (uint32_t i=0; i < N_DAC_PARAM; i++) {
        bram_index[i] = i;
        connected_bram[i] = true;
    }

    for (uint32_t i=N_DAC_PARAM; i < N_DAC_BRAM_PARAM; i++)
        connected_bram[i] = false;

    update_dac_routing();
}

int Oscillo::get_first_empty_bram_index() {
    for (uint32_t i=0; i < N_DAC_BRAM_PARAM; i++)
        if ((bram_index[0] != i) && (bram_index[1] != i))
            return i;
    return -1;
}

void Oscillo::update_dac_routing() {
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