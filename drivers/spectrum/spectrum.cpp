/// (c) Koheron

#include "spectrum.hpp"

#include <thread>
#include <chrono>

Spectrum::Spectrum(DevMem& dvm)
: cfg(dvm.get<CONFIG_MEM>())
, sts(dvm.get<STATUS_MEM>())
, spectrum_map(dvm.get<SPECTRUM_MEM>())
, demod_map(dvm.get<DEMOD_MEM>())
, noise_floor_map(dvm.get<NOISE_FLOOR_MEM>())
, spectrum_decim(0)
, fifo(dvm)
, dac(dvm)
{
    raw_data = spectrum_map.get_ptr<float>();
    set_averaging(true);
    cfg.write<ADDR_OFF>(19 << 2); // set tvalid delay to 19 * 8 ns
    set_address_range(0, WFM_SIZE);
    set_period(WFM_SIZE);
    set_n_avg_min(0);
    dac.set_config_reg(DAC_SELECT_OFF, ADDR_SELECT_OFF);
}

std::array<float, WFM_SIZE>& Spectrum::get_spectrum()
{
    cfg.set_bit<ADDR_OFF, 1>();
    wait_for_acquisition();

    if (sts.read<AVG_ON_OUT_OFF>()) {
        float num_avg = float(get_num_average());
        for (unsigned int i=0; i<WFM_SIZE; i++)
            spectrum_data[i] = raw_data[i] / num_avg;
    } else {
        for (unsigned int i=0; i<WFM_SIZE; i++)
            spectrum_data[i] = raw_data[i];
    }

    cfg.clear_bit<ADDR_OFF, 1>();
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

    cfg.set_bit<ADDR_OFF, 1>();
    uint32_t n_pts = (index_high - index_low)/decim_factor;
    spectrum_decim.resize(n_pts);
    wait_for_acquisition();

    if (sts.read<AVG_ON_OUT_OFF>()) {
        float num_avg = float(get_num_average());

        for (unsigned int i=0; i<spectrum_decim.size(); i++)
            spectrum_decim[i] = raw_data[index_low + decim_factor * i] / num_avg;
    } else {
        for (unsigned int i=0; i<spectrum_decim.size(); i++)
            spectrum_decim[i] = raw_data[index_low + decim_factor * i];
    }

    cfg.clear_bit<ADDR_OFF, 1>();
    return spectrum_decim;
}

void Spectrum::set_averaging(bool avg_on)
{
    if (avg_on)
        cfg.set_bit<AVG_OFF, 0>();
    else
        cfg.clear_bit<AVG_OFF, 0>();
}

/////////////////////
// Peak detection
/////////////////////

// Peak detection happens only between address_low and address_high
void Spectrum::set_address_range(uint32_t address_low, uint32_t address_high)
{
    cfg.write<PEAK_ADDRESS_LOW_OFF>(address_low);
    cfg.write<PEAK_ADDRESS_HIGH_OFF>(address_high);
    cfg.write<PEAK_ADDRESS_RESET_OFF>((address_low + WFM_SIZE - 1) % WFM_SIZE);
}
