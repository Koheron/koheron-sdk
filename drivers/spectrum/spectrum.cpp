/// (c) Koheron

#include "spectrum.hpp"

#include <thread>
#include <chrono>

Spectrum::Spectrum(Klib::DevMem& dvm_)
: dvm(dvm_)
, spectrum_decim(0)
, fifo(dvm_)
, dac(dvm_, dac_brams)
{
    config_map      = dvm.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
    status_map      = dvm.AddMemoryMap(STATUS_ADDR, STATUS_RANGE, PROT_READ);
    spectrum_map    = dvm.AddMemoryMap(SPECTRUM_ADDR, SPECTRUM_RANGE);
    demod_map       = dvm.AddMemoryMap(DEMOD_ADDR, DEMOD_RANGE);
    noise_floor_map = dvm.AddMemoryMap(NOISE_FLOOR_ADDR, NOISE_FLOOR_RANGE);
    peak_fifo_map   = dvm.AddMemoryMap(PEAK_FIFO_ADDR, PEAK_FIFO_RANGE);

    raw_data = dvm.read_buffer<float>(spectrum_map);

    fifo.set_map(peak_fifo_map);

    set_averaging(true);

    // set tvalid delay to 19 * 8 ns
    dvm.write32(config_map, ADDR_OFF, 19 << 2);

    set_address_range(0, WFM_SIZE);
    set_period(WFM_SIZE);
    set_n_avg_min(0);

    dac.set_config_reg(config_map, DAC_SELECT_OFF, ADDR_SELECT_OFF);
}

std::array<float, WFM_SIZE>& Spectrum::get_spectrum()
{
    dvm.set_bit(config_map,ADDR_OFF, 1);
    wait_for_acquisition();

    if (dvm.read32(status_map, AVG_ON_OUT_OFF)) {
        float num_avg = float(get_num_average());
        for (unsigned int i=0; i<WFM_SIZE; i++)
            spectrum_data[i] = raw_data[i] / num_avg;
    } else {
        for (unsigned int i=0; i<WFM_SIZE; i++)
            spectrum_data[i] = raw_data[i];
    }

    dvm.clear_bit(config_map, ADDR_OFF, 1);
    return spectrum_data;
}

std::vector<float>& Spectrum::get_spectrum_decim(uint32_t decim_factor, uint32_t index_low, 
                                                 uint32_t index_high)
{
    // Sanity checks
    if (index_high <= index_low || index_high >= WFM_SIZE) {
        spectrum_decim.resize(0);
        return spectrum_decim;
    }

    dvm.set_bit(config_map, ADDR_OFF, 1);
    uint32_t n_pts = (index_high - index_low)/decim_factor;
    spectrum_decim.resize(n_pts);
    wait_for_acquisition();

    if (dvm.read32(status_map, AVG_ON_OUT_OFF)) {
        float num_avg = float(get_num_average());

        for (unsigned int i=0; i<spectrum_decim.size(); i++)
            spectrum_decim[i] = raw_data[index_low + decim_factor * i] / num_avg;
    } else {
        for (unsigned int i=0; i<spectrum_decim.size(); i++)
            spectrum_decim[i] = raw_data[index_low + decim_factor * i];
    }

    dvm.clear_bit(config_map, ADDR_OFF, 1);
    return spectrum_decim;
}

void Spectrum::set_averaging(bool avg_on)
{
    if (avg_on)
        dvm.set_bit(config_map, AVG_OFF, 0);
    else
        dvm.clear_bit(config_map, AVG_OFF, 0);
}

/////////////////////
// Peak detection
/////////////////////

// Peak detection happens only between address_low and address_high
void Spectrum::set_address_range(uint32_t address_low, uint32_t address_high)
{
    dvm.write32(config_map, PEAK_ADDRESS_LOW_OFF, address_low);
    dvm.write32(config_map, PEAK_ADDRESS_HIGH_OFF, address_high);
    dvm.write32(config_map, PEAK_ADDRESS_RESET_OFF, (address_low+WFM_SIZE-1) % WFM_SIZE);
}
