/// (c) Koheron

#include "spectrum.hpp"

#include <thread>
#include <chrono>

Spectrum::Spectrum(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
, data(0)
{
    samples_num = 0;
    status = CLOSED;
}

Spectrum::~Spectrum()
{
    Close();
}

int Spectrum::Open(uint32_t samples_num_)
{
    // Reopening
    if(status == OPENED && samples_num_ != samples_num) {
        Close();
    }

    if(status == CLOSED) {
        samples_num = samples_num_;
    
        // Acquisition time in microseconds
        // Factor two because depending whether TRIG_ACQ
        // is received at the beginning or the end of a
        // period the acquisition time can be twice as long
        acq_time_us = 20*(samples_num*1E6)/SAMPLING_RATE;
    
        config_map = dev_mem.AddMemoryMap(CONFIG_ADDR, 16*MAP_SIZE);
        
        if(static_cast<int>(config_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        status_map = dev_mem.AddMemoryMap(STATUS_ADDR, 16*MAP_SIZE);
        
        if(static_cast<int>(status_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        spectrum_map = dev_mem.AddMemoryMap(SPECTRUM_ADDR, 16*MAP_SIZE);
        
        if(static_cast<int>(spectrum_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        demod_map = dev_mem.AddMemoryMap(DEMOD_ADDR, 16*MAP_SIZE);
        
        if(static_cast<int>(demod_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        raw_data = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(spectrum_map));
        data = Klib::KVector<float>(samples_num, 0);
        
        Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+AVG_OFF_OFF, 0);
        
        status = OPENED;
    }
    
    return 0;
}

void Spectrum::Close()
{
    if(status == OPENED) {
        dev_mem.RmMemoryMap(config_map);
        dev_mem.RmMemoryMap(status_map);
        dev_mem.RmMemoryMap(spectrum_map);
        dev_mem.RmMemoryMap(demod_map);
        status = CLOSED;
    }
}

void Spectrum::set_scale_sch(uint32_t scale_sch)
{
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + CFG_FFT_OFF, 
                     1 + 2 * scale_sch);
}

void Spectrum::set_offset(uint32_t offset_real, uint32_t offset_imag)
{
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + SUBSTRACT_MEAN_OFF, 
                     offset_real + 16384 * offset_imag);
}

void Spectrum::_wait_for_acquisition()
{
    // The overhead of sleep_for might be of the order of our waiting time:
    // http://stackoverflow.com/questions/18071664/stdthis-threadsleep-for-and-nanoseconds
    std::this_thread::sleep_for(std::chrono::microseconds(acq_time_us));
}

Klib::KVector<float>& Spectrum::get_spectrum()
{
    Klib::SetBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    
    _wait_for_acquisition();
    
    uint32_t n_avg = get_num_average();

    for(unsigned int i=0; i<data.size(); i++)
        data[i] = raw_data[i] / float(n_avg);
//            data[i] = raw_data[i];
    
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map)+ADDR_OFF, 1);
    return data;
}

uint32_t Spectrum::get_num_average()
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(status_map)+N_AVG1_OFF);
}

