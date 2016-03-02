/// (c) Koheron

#include "oscillo.hpp"

#include <thread>
#include <chrono>

Oscillo::Oscillo(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
, data(0)
, data_all(0)
, data_zeros(0)
, data_all_int(0)
{
    avg_on = false;
    
    waveform_size = 0;
    status = CLOSED;
}
 
Oscillo::~Oscillo()
{
    Close();
}

int Oscillo::Open(uint32_t waveform_size_)
{
    // Reopening
    if(status == OPENED && waveform_size_ != waveform_size) {
        Close();
    }

    if(status == CLOSED) {
        waveform_size = waveform_size_;
        
        // Acquisition time in microseconds
        // Factor two because depending whether TRIG_ACQ
        // is received at the beginning or the end of a
        // period the acquisition time can be twice as long
        acq_time_us = 2*(waveform_size*1E6)/SAMPLING_RATE;
    
        config_map = dev_mem.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
        
        if(static_cast<int>(config_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        status_map = dev_mem.AddMemoryMap(STATUS_ADDR, STATUS_RANGE);
        
        if(static_cast<int>(status_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        adc_1_map = dev_mem.AddMemoryMap(ADC1_ADDR, ADC1_RANGE);
        
        if(static_cast<int>(adc_1_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        adc_2_map = dev_mem.AddMemoryMap(ADC2_ADDR, ADC2_RANGE);
        
        if(static_cast<int>(adc_2_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        data = std::vector<float>(waveform_size, 0);
        data_all = std::vector<float>(2*waveform_size, 0);
        data_zeros = std::vector<float>(2*waveform_size, 0);
        data_all_int = std::vector<uint32_t>(waveform_size, 0);

        status = OPENED;
        
        // Reset averaging
        set_averaging(false);
    }
    
    return 0;
}

void Oscillo::Close()
{
    if(status == OPENED) {
        dev_mem.RmMemoryMap(config_map);
        dev_mem.RmMemoryMap(status_map);
        dev_mem.RmMemoryMap(adc_1_map);
        dev_mem.RmMemoryMap(adc_2_map);
        status = CLOSED;
    }
}

void Oscillo::_wait_for_acquisition()
{
    // The overhead of sleep_for might be of the order of our waiting time:
    // http://stackoverflow.com/questions/18071664/stdthis-threadsleep-for-and-nanoseconds
    std::this_thread::sleep_for(std::chrono::microseconds(acq_time_us));
}

// http://stackoverflow.com/questions/12276675/modulus-with-negative-numbers-in-c
long long int mod(long long int k, long long int n)
{
    return ((k %= n) < 0) ? k+n : k;
}

#define POW_2_31 2147483648 // 2^31
#define POW_2_32 4294967296 // 2^32

float _raw_to_float(uint32_t raw)
{
    return float(mod(raw - POW_2_31, POW_2_32) - POW_2_31);
}

void Oscillo::_raw_to_vector(uint32_t *raw_data)
{    
    if(avg_on) {
        uint32_t num_avg 
            = Klib::ReadReg32(dev_mem.GetBaseAddr(status_map)+N_AVG1_OFF);
    
        for(unsigned int i=0; i<data.size(); i++)
            data[i] = _raw_to_float(raw_data[i]) / float(num_avg);
    } else {
        for(unsigned int i=0; i<data.size(); i++)
            data[i] = _raw_to_float(raw_data[i]);
    }
}

void Oscillo::_raw_to_vector_all(uint32_t *raw_data_1, uint32_t *raw_data_2)
{    
    if(avg_on) {
        uint32_t num_avg 
            = Klib::ReadReg32(dev_mem.GetBaseAddr(status_map)+N_AVG1_OFF);
    
        for(unsigned int i=0; i<waveform_size; i++) {
            data_all[i] = _raw_to_float(raw_data_1[i]) / float(num_avg);
            data_all[i + waveform_size] 
                = _raw_to_float(raw_data_2[i]) / float(num_avg);
        }
    } else {
        for(unsigned int i=0; i<waveform_size; i++) {
            data_all[i] = _raw_to_float(raw_data_1[i]);
            data_all[i + waveform_size] = _raw_to_float(raw_data_2[i]);
        }
    }
}

void Oscillo::_raw_to_vector_all_raw(uint32_t *raw_data_1, uint32_t *raw_data_2)
{    
    for(unsigned int i=0; i<waveform_size; i++) {
        data_all[i] = raw_data_1[i];
        data_all[i + waveform_size] = raw_data_2[i];
    }
}


std::vector<float>& Oscillo::read_data(bool channel)
{
    Klib::MemMapID adc_map;
    channel ? adc_map = adc_1_map : adc_map = adc_2_map;

    Klib::SetBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    
    _wait_for_acquisition();
    
    uint32_t *raw_data 
        = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_map));
    _raw_to_vector(raw_data);

    Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    return data;
}

std::vector<float>& Oscillo::read_all_channels()
{
    Klib::SetBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    
    _wait_for_acquisition();
    
    uint32_t *raw_data_1
        = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_1_map));
    uint32_t *raw_data_2
        = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_2_map));
        
    _raw_to_vector_all(raw_data_1, raw_data_2);

    Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    return data_all;
}

void Oscillo::set_averaging(bool avg_status)
{
    avg_on = avg_status;
    
    if(avg_on) {
        Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+AVG0_OFF, 0);
        Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+AVG1_OFF, 0);
    } else {
        Klib::SetBit(dev_mem.GetBaseAddr(config_map)+AVG0_OFF, 0);
        Klib::SetBit(dev_mem.GetBaseAddr(config_map)+AVG1_OFF, 0);
    }
}

uint32_t Oscillo::get_num_average()
{
    return avg_on ? Klib::ReadReg32(dev_mem.GetBaseAddr(status_map)+N_AVG1_OFF) : 0;
}

//////////////////////////////////////////////////////////
// Functions used for speed test :
//////////////////////////////////////////////////////////

std::vector<float>& Oscillo::read_raw_all()
{
    Klib::SetBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    
    //_wait_for_acquisition();
    
    uint32_t *raw_data_1
        = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_1_map));
    uint32_t *raw_data_2
        = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_2_map));
        
    _raw_to_vector_all_raw(raw_data_1, raw_data_2);

    Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    return data_all;
}

std::vector<float>& Oscillo::read_zeros()
{
    return data_zeros;
}

std::vector<uint32_t> Oscillo::speed_test(uint32_t n_outer_loop, uint32_t n_inner_loop, uint32_t n_pts)
{
    std::vector<uint32_t> durations(n_outer_loop);
    for(uint32_t j = 0; j < n_outer_loop; ++j) {
        auto begin = std::chrono::high_resolution_clock::now();
        for(uint32_t i = 0; i < n_inner_loop; ++i) {
            uint32_t *raw_data_1 = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_1_map));
            for(unsigned int i=0; i<n_pts; i++) {
                data_all_int[i] = raw_data_1[i];
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        durations[j] = std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count();
    }
    return durations;
}


