/// DAC router
///
/// (c) Koheron

#ifndef __DRIVERS_LIB_DAC_ROUTER_HPP__
#define __DRIVERS_LIB_DAC_ROUTER_HPP__

#include <math.h>
#include <context.hpp>

constexpr uint32_t get_width(uint32_t n_dac_bram) {
    return ceil(log(float(n_dac_bram)) / log(2.));
}
static_assert(get_width(8) == 3, "get_width test failed");
static_assert(get_width(9) == 4, "get_width test failed");

template<uint32_t n_dac, uint32_t n_dac_bram>
class DacRouter
{
  public:
    DacRouter(Context& ctx)
    : cfg(ctx.mm.get<mem::config>())
    , dac_map(ctx.mm.get<mem::dac>())
    {}

    void set_config_reg(uint32_t dac_select_off_, uint32_t addr_select_off_) {
        dac_select_off = dac_select_off_;
        addr_select_off = addr_select_off_;
        init_dac_brams();
    }

    template<size_t N>
    std::array<uint32_t, N>& get_data(uint32_t channel) {
        return dac_map.read_array<uint32_t, N>(bram_index[channel]);
    }

    /// /!\ Array is of size 1/2 of the number of samples in the waveform
    template<size_t N>
    void set_data(uint32_t channel, const std::array<uint32_t, N> arr) {
        uint32_t new_idx = get_first_empty_bram();
        dac_map.set_ptr(arr.data(), N, new_idx);
        bram_index[channel] = new_idx;
        update_dac_routing();
    }

  private:
    Memory<mem::config>& cfg;
    uint32_t dac_select_off;
    uint32_t addr_select_off;
    Memory<mem::dac>& dac_map;
    std::array<uint32_t, n_dac> bram_index;

    void init_dac_brams() {
        // Use BRAM0 for DAC0, BRAM1 for DAC1 ...
        for (uint32_t i=0; i < n_dac; i++) {
            bram_index[i] = i;
        }
        update_dac_routing();
    }

    void update_dac_routing() {
        // dac_select defines the connection between BRAMs and DACs
        uint32_t dac_select = 0;
        for (uint32_t i=0; i < n_dac; i++) {
            dac_select += bram_index[i] << (get_width(n_dac_bram) * i);
        }
        cfg.write_reg(dac_select_off, dac_select);

        // addr_select defines the connection between address generators and BRAMs
        uint32_t addr_select = 0;
        for (uint32_t j=0; j < n_dac_bram; j++) {
            addr_select += get_addr_index(j) << (get_width(n_dac) * j);
        }
        cfg.write_reg(addr_select_off, addr_select);
    }

    uint32_t get_addr_index(uint32_t i) {
        for (uint32_t j=0; j < n_dac; j++) {
            if (bram_index[j] == i) {
                return j;
            }
        }
        return 0;
    }

    uint32_t get_first_empty_bram() {
        for (uint32_t i=0; i < n_dac_bram; i++) {
            // Check if the BRAM is connected to no
            bool empty = true;
            for (uint32_t j=0; j < n_dac; j++) {
                if (bram_index[j] == i) {
                    empty = false;
                    break;
                }
            }
            if (empty) {
                return i;
            }
        }
        return -1;
    }

};

#endif // __DRIVERS_LIB_DAC_ROUTER_HPP__
