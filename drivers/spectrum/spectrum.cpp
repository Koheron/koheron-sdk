/// (c) Koheron

#include "spectrum.hpp"

#include <thread>
#include <chrono>

Spectrum::Spectrum(Klib::DevMem& dvm_)
: dvm(dvm_)
, spectrum_decim(0)
{
    status = CLOSED;
}

int Spectrum::Open()
{
    if (status == CLOSED) {
        auto ids = dvm.RequestMemoryMaps<7>({{
            { CONFIG_ADDR      , CONFIG_RANGE      },
            { STATUS_ADDR      , STATUS_RANGE      },
            { SPECTRUM_ADDR    , SPECTRUM_RANGE    },
            { DEMOD_ADDR       , DEMOD_RANGE       },
            { NOISE_FLOOR_ADDR , NOISE_FLOOR_RANGE },
            { PEAK_FIFO_ADDR   , PEAK_FIFO_RANGE   },
            { DAC_ADDR         , DAC_RANGE         }
        }});

        if (dvm.CheckMapIDs(ids) < 0) {
            status = FAILED;
            return -1;
        }

        config_map      = ids[0];
        status_map      = ids[1];
        spectrum_map    = ids[2];
        demod_map       = ids[3];
        noise_floor_map = ids[4];
        peak_fifo_map   = ids[5];
        dac_map         = ids[6];
        
        raw_data = reinterpret_cast<float*>(dvm.GetBaseAddr(spectrum_map));
        fifo.set_address(dvm.GetBaseAddr(peak_fifo_map));
        
        set_averaging(true);
        set_address_range(0, WFM_SIZE);
        set_period(WFM_SIZE);
        set_n_avg_min(0);

        status = OPENED;
    }
    
    return 0;
}

void Spectrum::set_period(uint32_t period)
{
    dvm.write32(config_map, PERIOD0_OFF, period - 1);
    dvm.write32(config_map, THRESHOLD0_OFF, period - 6);
}

void Spectrum::set_n_avg_min(uint32_t n_avg_min) {
    uint32_t n_avg_min_ = (n_avg_min < 2) ? 0 : n_avg_min-2;
    dvm.write32(config_map, N_AVG_MIN0_OFF, n_avg_min_);
}

void Spectrum::reset()
{
    assert(status == OPENED);
    // Config
    dvm.clear_bit(config_map, ADDR_OFF, 1);
    dvm.set_bit(config_map, ADDR_OFF, 0);
}

void Spectrum::set_dac_buffer(const uint32_t *data, uint32_t len)
{
    for (uint32_t i=0; i<len; i++)
        dvm.write32(dac_map, sizeof(uint32_t) * i, data[i]);
}

void Spectrum::reset_acquisition()
{
    dvm.clear_bit(config_map, ADDR_OFF, 1);
    dvm.set_bit(config_map, ADDR_OFF, 1);
}

void Spectrum::set_scale_sch(uint32_t scale_sch)
{
    // LSB at 1 for forward FFT
    dvm.write32(config_map, CFG_FFT_OFF, 1 + 2 * scale_sch);
}

void Spectrum::set_offset(uint32_t offset_real, uint32_t offset_imag)
{
    dvm.write32(config_map, SUBSTRACT_MEAN_OFF, offset_real + 16384 * offset_imag);
}

void Spectrum::set_demod_buffer(const uint32_t *data, uint32_t len)
{
    for (uint32_t i=0; i<len; i++)
        dvm.write32(demod_map, sizeof(uint32_t)*i, data[i]);
}

void Spectrum::set_noise_floor_buffer(const uint32_t *data, uint32_t len)
{
    for (uint32_t i=0; i<len; i++)
        dvm.write32(noise_floor_map, sizeof(uint32_t) * i, data[i]);
}

void Spectrum::_wait_for_acquisition()
{
    uint32_t ready;
    do {
        ready = dvm.read32(status_map, AVG_READY_OFF);
    } while (ready == 0);
}

std::array<float, WFM_SIZE>& Spectrum::get_spectrum()
{
    dvm.set_bit(config_map,ADDR_OFF, 1);    
    _wait_for_acquisition();
    uint32_t avg_on = bool(dvm.read32(status_map, AVG_ON_OUT_OFF));

    if (avg_on) {
        float num_avg = float(get_num_average());
        for(unsigned int i=0; i<WFM_SIZE; i++)
            spectrum_data[i] = raw_data[i] / num_avg;
    } else {
        for(unsigned int i=0; i<WFM_SIZE; i++)
            spectrum_data[i] = raw_data[i];
    }
    dvm.clear_bit(config_map, ADDR_OFF, 1);
    return spectrum_data;
}

std::vector<float>& Spectrum::get_spectrum_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high)
{
    // Sanity checks
    if (index_high <= index_low || index_high >= WFM_SIZE) {
        spectrum_decim.resize(0);
        return spectrum_decim;
    }

    dvm.set_bit(config_map, ADDR_OFF, 1);
    uint32_t n_pts = (index_high - index_low)/decim_factor;
    spectrum_decim.resize(n_pts);
    _wait_for_acquisition();
    uint32_t avg_on = bool(dvm.read32(status_map, AVG_ON_OUT_OFF));

    if (avg_on) {
        float num_avg = float(get_num_average());

        for(unsigned int i=0; i<spectrum_decim.size(); i++)
            spectrum_decim[i] = raw_data[index_low + decim_factor * i] / num_avg;
    } else {
        for(unsigned int i=0; i<spectrum_decim.size(); i++)
            spectrum_decim[i] = raw_data[index_low + decim_factor * i];
    }

    dvm.clear_bit(config_map, ADDR_OFF, 1);
    return spectrum_decim;
}

void Spectrum::set_averaging(bool avg_on)
{
    if(avg_on)
        dvm.set_bit(config_map, AVG_ON_OFF, 0);
    else
        dvm.clear_bit(config_map, AVG_ON_OFF, 0);
}

uint32_t Spectrum::get_num_average()
{
    return dvm.read32(status_map, N_AVG_OFF);
}

/////////////////////
// Peak detection
/////////////////////

uint32_t Spectrum::get_peak_address()
{
    return dvm.read32(status_map, PEAK_ADDRESS_OFF);
}

uint32_t Spectrum::get_peak_maximum()
{
    return dvm.read32(status_map, PEAK_MAXIMUM_OFF);
}

// Peak detection happens only between address_low and address_high
void Spectrum::set_address_range(uint32_t address_low, uint32_t address_high)
{
    dvm.write32(config_map, PEAK_ADDRESS_LOW_OFF, address_low);
    dvm.write32(config_map, PEAK_ADDRESS_HIGH_OFF, address_high);
    dvm.write32(config_map, + PEAK_ADDRESS_RESET_OFF, (address_low+WFM_SIZE-1) % WFM_SIZE);
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
