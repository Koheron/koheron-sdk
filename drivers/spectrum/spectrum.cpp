/// (c) Koheron

#include "spectrum.hpp"

#include <thread>
#include <chrono>

Spectrum::Spectrum(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
, spectrum_decim(0)
{
    status = CLOSED;
}

int Spectrum::Open()
{
    if (status == CLOSED) {
        auto ids = dev_mem.RequestMemoryMaps<6>({{
            { CONFIG_ADDR      , CONFIG_RANGE      },
            { STATUS_ADDR      , STATUS_RANGE      },
            { SPECTRUM_ADDR    , SPECTRUM_RANGE    },
            { DEMOD_ADDR       , DEMOD_RANGE       },
            { NOISE_FLOOR_ADDR , NOISE_FLOOR_RANGE },
            { PEAK_FIFO_ADDR   , PEAK_FIFO_RANGE   }
        }});

        if (dev_mem.CheckMapIDs(ids) < 0) {
            status = FAILED;
            return -1;
        }

        config_map      = ids[0];
        status_map      = ids[1];
        spectrum_map    = ids[2];
        demod_map       = ids[3];
        noise_floor_map = ids[4];
        peak_fifo_map   = ids[5];
        
        raw_data = reinterpret_cast<float*>(dev_mem.GetBaseAddr(spectrum_map));
        fifo.set_address(dev_mem.GetBaseAddr(peak_fifo_map));
        
        set_averaging(true);
        set_address_range(0, WFM_SIZE);
        status = OPENED;
    }
    
    return 0;
}

void Spectrum::set_scale_sch(uint32_t scale_sch)
{
    // LSB at 1 for forward FFT
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + CFG_FFT_OFF, 1 + 2 * scale_sch);
}

void Spectrum::set_offset(uint32_t offset_real, uint32_t offset_imag)
{
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + SUBSTRACT_MEAN_OFF, offset_real + 16384 * offset_imag);
}

void Spectrum::set_demod_buffer(const uint32_t *data, uint32_t len)
{
    for (uint32_t i=0; i<len; i++)
        Klib::WriteReg32(dev_mem.GetBaseAddr(demod_map)+sizeof(uint32_t)*i, data[i]);
}

void Spectrum::set_noise_floor_buffer(const uint32_t *data, uint32_t len)
{
    for (uint32_t i=0; i<len; i++)
        Klib::WriteReg32(dev_mem.GetBaseAddr(noise_floor_map) + sizeof(uint32_t) * i, data[i]);
}

void Spectrum::_wait_for_acquisition()
{
    // The overhead of sleep_for might be of the order of our waiting time:
    // http://stackoverflow.com/questions/18071664/stdthis-threadsleep-for-and-nanoseconds
    std::this_thread::sleep_for(std::chrono::microseconds(ACQ_TIME_US));
}

std::array<float, WFM_SIZE>& Spectrum::get_spectrum()
{
    Klib::SetBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 1);    
    _wait_for_acquisition();
    if (avg_on) {
        float num_avg = float(get_num_average());
        for(unsigned int i=0; i<WFM_SIZE; i++)
            spectrum_data[i] = raw_data[i] / num_avg;
    } else {
        for(unsigned int i=0; i<WFM_SIZE; i++)
            spectrum_data[i] = raw_data[i];
    }
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 1);
    return spectrum_data;
}

std::vector<float>& Spectrum::get_spectrum_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high)
{
    // Sanity checks
    if (index_high <= index_low || index_high >= WFM_SIZE) {
        spectrum_decim.resize(0);
        return spectrum_decim;
    }

    Klib::SetBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 1);
    uint32_t n_pts = (index_high - index_low)/decim_factor;
    spectrum_decim.resize(n_pts);
    _wait_for_acquisition();

    if (avg_on) {
        float num_avg = float(get_num_average());

        for(unsigned int i=0; i<spectrum_decim.size(); i++)
            spectrum_decim[i] = raw_data[index_low + decim_factor * i] / num_avg;
    } else {
        for(unsigned int i=0; i<spectrum_decim.size(); i++)
            spectrum_decim[i] = raw_data[index_low + decim_factor * i];
    }

    Klib::ClearBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 1);
    return spectrum_decim;
}

void Spectrum::set_averaging(bool avg_status)
{
    avg_on = avg_status;
    
    if(avg_on) {
        Klib::ClearBit(dev_mem.GetBaseAddr(config_map) + AVG_OFF_OFF, 0);
    } else {
        Klib::SetBit(dev_mem.GetBaseAddr(config_map) + AVG_OFF_OFF, 0);
    }
}

uint32_t Spectrum::get_num_average()
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(status_map) + N_AVG_OFF);
}

/////////////////////
// Peak detection
/////////////////////

uint32_t Spectrum::get_peak_address()
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(status_map) + PEAK_ADDRESS_OFF);
}

uint32_t Spectrum::get_peak_maximum()
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(status_map) + PEAK_MAXIMUM_OFF);
}

// Peak detection happens only between address_low and address_high
void Spectrum::set_address_range(uint32_t address_low, uint32_t address_high)
{
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + PEAK_ADDRESS_LOW_OFF, address_low);
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + PEAK_ADDRESS_HIGH_OFF, address_high);
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + PEAK_ADDRESS_RESET_OFF, (address_low+WFM_SIZE-1) % WFM_SIZE);
}

// Read the peak data stream

void Spectrum::fifo_start_acquisition(uint32_t acq_period)
{
    fifo.start_acquisition(acq_period);
}

void Spectrum::fifo_stop_acquisition()
{
    fifo.stop_acquisition();
}

bool Spectrum::fifo_get_acquire_status()
{
    return fifo.get_acquire_status();
}

uint32_t Spectrum::store_peak_fifo_data()
{
    return fifo.get_buffer_length();
}

std::vector<uint32_t>& Spectrum::get_peak_fifo_data()
{
    return fifo.get_data();
}

uint32_t Spectrum::get_peak_fifo_length()
{
    return fifo.get_fifo_length();
}
