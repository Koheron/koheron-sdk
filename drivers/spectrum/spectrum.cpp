/// (c) Koheron

#include "spectrum.hpp"

#include <thread>
#include <chrono>

Spectrum::Spectrum(DevMem& dvm)
: cfg(dvm.get<mem::config>())
, sts(dvm.get<mem::status>())
, spectrum_map(dvm.get<mem::spectrum>())
, demod_map(dvm.get<mem::demod>())
, noise_floor_map(dvm.get<mem::noise_floor>())
, spectrum_decim(0)
, fifo(dvm)
, dac(dvm)
{
    raw_data = spectrum_map.get_ptr<float>();
    set_averaging(true);
    cfg.write<reg::addr>(19 << 2); // set tvalid delay to 19 * 8 ns
    set_address_range(0, WFM_SIZE);
    set_period(WFM_SIZE);
    set_n_avg_min(0);
    dac.set_config_reg(reg::dac_select, reg::addr_select);
}

std::array<float, WFM_SIZE>& Spectrum::get_spectrum()
{
    cfg.set_bit<reg::addr, 1>();
    wait_for_acquisition();

    if (sts.read<reg::avg_on_out>()) {
        float num_avg = float(get_num_average());
        for (unsigned int i=0; i<WFM_SIZE; i++)
            spectrum_data[i] = raw_data[i] / num_avg;
    } else {
        for (unsigned int i=0; i<WFM_SIZE; i++)
            spectrum_data[i] = raw_data[i];
    }

    cfg.clear_bit<reg::addr, 1>();
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

    cfg.set_bit<reg::addr, 1>();
    uint32_t n_pts = (index_high - index_low)/decim_factor;
    spectrum_decim.resize(n_pts);
    wait_for_acquisition();

    if (sts.read<reg::avg_on_out>()) {
        float num_avg = float(get_num_average());

        for (unsigned int i=0; i<spectrum_decim.size(); i++)
            spectrum_decim[i] = raw_data[index_low + decim_factor * i] / num_avg;
    } else {
        for (unsigned int i=0; i<spectrum_decim.size(); i++)
            spectrum_decim[i] = raw_data[index_low + decim_factor * i];
    }

    cfg.clear_bit<reg::addr, 1>();
    return spectrum_decim;
}

void Spectrum::set_averaging(bool avg_on)
{
    if (avg_on)
        cfg.set_bit<reg::avg, 0>();
    else
        cfg.clear_bit<reg::avg, 0>();
}

/////////////////////
// Peak detection
/////////////////////

// Peak detection happens only between address_low and address_high
void Spectrum::set_address_range(uint32_t address_low, uint32_t address_high)
{
    cfg.write<reg::peak_address_low>(address_low);
    cfg.write<reg::peak_address_high>(address_high);
    cfg.write<reg::peak_address_reset>((address_low + WFM_SIZE - 1) % WFM_SIZE);
}
