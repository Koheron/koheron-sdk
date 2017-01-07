/// Oscilloscope driver
///
/// (c) Koheron

#ifndef __DRIVERS_OSCILLO_HPP__
#define __DRIVERS_OSCILLO_HPP__

#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>

#include <context.hpp>
#include <drivers/lib/dac_router.hpp>

#define SAMPLING_RATE 125E6
#define WFM_SIZE mem::adc_range/sizeof(float)
#define ACQ_PERIOD_NS 8 // Duration between two acquisitions (ns)

constexpr auto wfm_time = std::chrono::nanoseconds(WFM_SIZE * static_cast<uint32_t>(1E9 / SAMPLING_RATE));
constexpr std::array<uint32_t, 2> n_avg_offset = {reg::n_avg0, reg::n_avg1};

class Oscillo
{
  public:
    Oscillo(Context& ctx);

    // Reset ...

    void reset() {
        cfg.clear_bit<reg::addr, 0>();
        cfg.set_bit<reg::addr, 0>();
    }

    void reset_acquisition() {
        cfg.clear_bit<reg::addr, 1>();
        cfg.set_bit<reg::addr, 1>();
    }

    void set_averaging(bool avg_on);

    // Monitoring

    uint64_t get_counter() {
        uint64_t lsb = sts.read<reg::counter0>();
        uint64_t msb = sts.read<reg::counter1>();
        return lsb + (msb << 32);
    }

    uint32_t get_num_average(uint32_t channel) {
        return sts.read_reg(n_avg_offset[channel]);
    }

    void set_clken_mask(bool clken_mask) {
        cfg.write_bit<reg::clken_mask, 0>(clken_mask);
    }

    void update_now() {
        cfg.set_bit<reg::clken_mask, 1>();
        cfg.clear_bit<reg::clken_mask, 1>();
    }

    void always_update() {
        cfg.set_bit<reg::clken_mask, 1>();
    }

    void set_n_avg_min(uint32_t n_avg_min) {
        n_avg_min_ = (n_avg_min < 2) ? 0 : n_avg_min-2;
        cfg.write<reg::n_avg_min0>(n_avg_min_);
        cfg.write<reg::n_avg_min1>(n_avg_min_);
    }

    void set_addr_select(uint32_t addr_select) {
        cfg.write<reg::addr_select>(addr_select);
    }

    void set_dac_periods(uint32_t dac_period0, uint32_t dac_period1) {
        cfg.write<reg::dac_period0>(dac_period0 - 1);
        cfg.write<reg::dac_period1>(dac_period1 - 1);
        reset();
    }

    void set_avg_period(uint32_t avg_period) {
        cfg.write<reg::avg_period>(avg_period - 1);
        cfg.write<reg::avg_threshold>(avg_period - 6);
        reset();
    }

    // DACs
    void set_dac_buffer(uint32_t channel, const std::array<uint32_t, WFM_SIZE/2>& arr) {
        dac.set_data(channel, arr);
    }

    auto& get_dac_buffer(uint32_t channel) {
        return dac.get_data<WFM_SIZE/2>(channel);
    }

    // Read ADC
    std::array<float, 2*WFM_SIZE>& read_all_channels();
    std::vector<float>& read_all_channels_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high);

  private:
    int32_t *raw_data[2] = {nullptr, nullptr};

    Memory<mem::config>& cfg;
    Memory<mem::status>& sts;
    Memory<mem::adc>& adc_map;

    // Acquired data buffers
    std::array<float, 2*WFM_SIZE> data_all;
    std::vector<float> data_decim;

    DacRouter<prm::n_dac, prm::n_dac_bram> dac;

    uint32_t n_avg_min_;

    // Internal functions
    void _wait_for_acquisition();
};

#endif // __DRIVERS_OSCILLO_HPP__
