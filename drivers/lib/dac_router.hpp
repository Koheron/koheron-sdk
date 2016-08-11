/// DAC router
///
/// (c) Koheron

#ifndef __DRIVERS_LIB_DAC_ROUTER_HPP__
#define __DRIVERS_LIB_DAC_ROUTER_HPP__

#include "dev_mem.hpp"

constexpr uint32_t dac_sel_width(uint32_t n_dac_bram) {
    return ceil(log(float(n_dac_bram)) / log(2.));
}

constexpr uint32_t bram_sel_width(uint32_t n_dac) {
    return ceil(log(float(n_dac)) / log(2.));
}

// http://stackoverflow.com/questions/12276675/modulus-with-negative-numbers-in-c
constexpr long mod(long a, long b) {
    return (a % b + b) % b;
}

template<uint32_t n_dac, uint32_t n_dac_bram>
class DacRouter
{
  public:
    DacRouter(DevMem& dvm_, std::array<std::array<uint32_t, 2>, n_dac_bram> dac_brams)
    : dvm(dvm_)
    {
        for (uint32_t i=0; i<n_dac_bram; i++)
            dac_map[i] = dvm.add_memory_map(dac_brams[i][0], dac_brams[i][1]);
    }

    void set_config_reg(MemMapID config_map_, uint32_t dac_select_off_,
                        uint32_t addr_select_off_) {
        config_map = config_map_;
        dac_select_off = dac_select_off_;
        addr_select_off = addr_select_off_;

        init_dac_brams();
    }

    uint32_t* get_data(uint32_t channel) {return dvm.read_buff32(dac_map[bram_index[channel]]);}

    template<size_t N>
    std::array<uint32_t, N>& get_data(uint32_t channel)
    {
        return dvm.read_buffer<uint32_t, N, 0>(dac_map[bram_index[channel]]);
    }

    void set_data(uint32_t channel, const uint32_t *buffer, uint32_t len);

    /// /!\ Array is of size 1/2 of the number of samples in the waveform
    template<size_t N>
    void set_data(uint32_t channel, const std::array<uint32_t, N> arr) {
        set_data(channel, arr.data(), N);
    }

    /// The waveform is an array of floats between -1 and 1.
    /// Array is of size the number of samples in the waveform.
    template<size_t N>
    void set_data(uint32_t channel, const std::array<float, N> arr) {
        static_assert(N % 2 == 0, "N must be an even number");
        std::array<uint32_t, N/2> data;

        for (uint32_t i=0, j=0; i<N; i+=2, j++)
            data[j] = convert_data(arr[i]) + (convert_data(arr[i + 1]) << 16);

        set_data(channel, data);
    }

  private:
    DevMem& dvm;
    MemMapID config_map;
    uint32_t dac_select_off;
    uint32_t addr_select_off;
    std::array<MemMapID, n_dac_bram> dac_map;

    std::array<uint32_t, n_dac> bram_index;
    std::array<bool, n_dac_bram> connected_bram;

    void init_dac_brams();
    void update_dac_routing();
    int get_first_empty_bram_index();
    void switch_interconnect(uint32_t channel, uint32_t old_idx, uint32_t new_idx);

    uint32_t convert_data(float val) {
        return mod(static_cast<uint32_t>(floor(8192 * val) + 8192), 16384) + 8192;
    }
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

#endif // __DRIVERS_LIB_DAC_ROUTER_HPP__
