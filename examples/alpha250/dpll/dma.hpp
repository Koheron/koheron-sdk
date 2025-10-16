/// DMA driver
///
/// (c) Koheron

#ifndef __ALPHA250_DPLL_DMA_HPP__
#define __ALPHA250_DPLL_DMA_HPP__

#include "server/runtime/syslog.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/drivers/dma-s2mm.hpp"
#include "boards/alpha250/drivers/clock-generator.hpp"

#include <cstdint>
#include <array>

class Dma
{
  public:
    Dma() {
        fs_adc = rt::get_driver<ClockGenerator>().get_adc_sampling_freq();
        set_cic_rate(prm::cic_decimation_rate_default);
        logf("DMA transfer duration = {} s\n", double(dma_transfer_duration));
    }

    void set_cic_rate(uint32_t rate) {
        if (rate < prm::cic_decimation_rate_min ||
            rate > prm::cic_decimation_rate_max) {
            log<ERROR>("DMA: CIC rate out of range\n");
            return;
        }

        cic_rate = rate;
        fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
        dma_transfer_duration = prm::n_pts / fs;
        hw::get_memory<mem::control>().write<reg::cic_rate>(cic_rate);
    }

    auto& get_data() {
        auto& dma = rt::get_driver<DmaS2MM>();
        dma.start_transfer(mem::ram_addr, sizeof(int32_t) * prm::n_pts);
        dma.wait_for_transfer(dma_transfer_duration);
        auto& ram = hw::get_memory<mem::ram>();
        return ram.read_array<int32_t, data_size, read_offset>();
    }

    uint32_t get_data_size() {
        return data_size;
    }

    uint32_t get_sampling_frequency() {
        return fs;
    }

  private:
    static constexpr uint32_t data_size = 1000000;
    static constexpr uint32_t read_offset = (prm::n_pts - data_size) / 2;

    uint32_t cic_rate;
    float fs_adc, fs;
    float dma_transfer_duration;

    std::array<int32_t, data_size> data;
};

#endif // __ALPHA250_DPLL_DMA_HPP__
