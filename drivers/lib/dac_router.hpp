/// DAC router
///
/// (c) Koheron

#ifndef __DRIVERS_LIB_DAC_ROUTER_HPP__
#define __DRIVERS_LIB_DAC_ROUTER_HPP__

#include "dev_mem.hpp"

namespace Klib {

constexpr uint32_t dac_sel_width(uint32_t n_dac_bram) {
    return ceil(log(float(n_dac_bram)) / log(2.));
}

constexpr uint32_t bram_sel_width(uint32_t n_dac) {
    return ceil(log(float(n_dac)) / log(2.));
}

template<uint32_t n_dac, uint32_t n_dac_bram>
struct DacRouter
{
    DacRouter(DevMem& dvm_, std::array<std::array<uint32_t, 2>, n_dac_bram> dac_brams)
    : dvm(dvm_)
    {
        for (uint32_t i=0; i<n_dac_bram; i++)
            dac_map[i] = dvm.AddMemoryMap(dac_brams[i][0], dac_brams[i][1]);
    }

    uint32_t get_idx(uint32_t channel) const {return bram_index[channel];}

    void set_config_reg(Klib::MemMapID config_map_, uint32_t dac_select_off_,
                        uint32_t addr_select_off_) {
        config_map = config_map_;
        dac_select_off = dac_select_off_;
        addr_select_off = addr_select_off_;

        init_dac_brams();
    }

    uint32_t* get_data(uint32_t channel) {return dvm.read_buff32(dac_map[bram_index[channel]]);}
    void set_data(uint32_t channel, const uint32_t *buffer, uint32_t len);

    template<size_t N>
    void set_data(uint32_t channel, const std::array<uint32_t, N> arr) {
        set_data(channel, arr.data(), arr.size());
    }

    void init_dac_brams();
    void update_dac_routing();
    int get_first_empty_bram_index();
    void switch_interconnect(uint32_t channel, uint32_t old_idx, uint32_t new_idx);

    DevMem& dvm;
    MemMapID config_map;
    uint32_t dac_select_off;
    uint32_t addr_select_off;
    std::array<MemMapID, n_dac_bram> dac_map;

    std::array<uint32_t, n_dac> bram_index;
    std::array<bool, n_dac_bram> connected_bram;
};

template<uint32_t n_dac, uint32_t n_dac_bram>
inline void DacRouter<n_dac, n_dac_bram>::init_dac_brams()
{
    // Use BRAM0 for DAC0, BRAM1 for DAC1 ...
    for (uint32_t i=0; i < n_dac; i++) {
        bram_index[i] = i;
        connected_bram[i] = true;
    }

    for (uint32_t i=n_dac; i < n_dac_bram; i++)
        connected_bram[i] = false;

    update_dac_routing();
}

template<uint32_t n_dac, uint32_t n_dac_bram>
inline void DacRouter<n_dac, n_dac_bram>::update_dac_routing()
{
    // dac_select defines the connection between BRAMs and DACs
    uint32_t dac_select = 0;
    for (uint32_t i=0; i < n_dac_bram; i++)
        dac_select += bram_index[i] << (dac_sel_width(n_dac_bram) * i);
    dvm.write32(config_map, dac_select_off, dac_select);

    // addr_select defines the connection between address generators and BRAMs
    uint32_t addr_select = 0;
    for (uint32_t j=0; j < n_dac_bram; j++)
        addr_select += j << (bram_sel_width(n_dac) * bram_index[j]);
    dvm.write32(config_map, addr_select_off, addr_select);
}

template<uint32_t n_dac, uint32_t n_dac_bram>
inline int DacRouter<n_dac, n_dac_bram>::get_first_empty_bram_index()
{
    for (uint32_t i=0; i < n_dac_bram; i++)
        if ((bram_index[0] != i) && (bram_index[1] != i))
            return i;
    return -1;
}

template<uint32_t n_dac, uint32_t n_dac_bram>
inline void DacRouter<n_dac, n_dac_bram>::switch_interconnect(
            uint32_t channel, uint32_t old_idx, uint32_t new_idx)
{
    bram_index[channel] = new_idx;
    connected_bram[new_idx] = true;
    update_dac_routing();
    connected_bram[old_idx] = false;
}

template<uint32_t n_dac, uint32_t n_dac_bram>
inline void DacRouter<n_dac, n_dac_bram>::set_data(
            uint32_t channel, const uint32_t *buffer, uint32_t len)
{
    uint32_t old_idx = bram_index[channel];
    uint32_t new_idx = get_first_empty_bram_index();
    dvm.write_buff32(dac_map[new_idx], 0, buffer, len);
    switch_interconnect(channel, old_idx, new_idx);
}

}; // namespace Klib

#endif // __DRIVERS_LIB_DAC_ROUTER_HPP__
