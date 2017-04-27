/// Oscilloscope driver
///
/// (c) Koheron

#ifndef __DRIVERS_OSCILLO_HPP__
#define __DRIVERS_OSCILLO_HPP__

#include <chrono>
#include <thread>
#include <cmath>

#include <context.hpp>

constexpr float PI = 3.1415927;
constexpr float SAMPLING_RATE = 125E6;
constexpr uint32_t WFM_SIZE = mem::adc_range/sizeof(float);

constexpr auto wfm_time = std::chrono::nanoseconds(WFM_SIZE * static_cast<uint32_t>(1E9F / SAMPLING_RATE));
constexpr std::array<uint32_t, 2> N_AVG_OFFSET = {reg::n_avg0, reg::n_avg1};

class Oscillo
{
  public:
    Oscillo(Context& ctx_)
    :ctx(ctx_)
    ,ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , adc_map(ctx.mm.get<mem::adc>())
    , decimated_data(0)
    {
        raw_data[0] = adc_map.get_ptr<int32_t>(0);
        raw_data[1] = adc_map.get_ptr<int32_t>(1);

        set_average(false); // Reset averaging
        ctl.write<reg::addr>(19 << 2); // set tvalid delay to 19 * 8 ns
        set_num_average_min(0);
        set_dac_periods(WFM_SIZE, WFM_SIZE);
        set_average_period(WFM_SIZE);
    }

    void reset() {
        ctl.clear_bit<reg::addr, 0>();
        ctl.set_bit<reg::addr, 0>();
    }

    void reset_acquisition() {
        ctl.clear_bit<reg::addr, 1>();
        ctl.set_bit<reg::addr, 1>();
    }

    // Averaging

    auto get_average_status() {
        return std::make_tuple(
            is_average,
            num_average_min,
            num_average
        );
    }

    void set_average(bool is_average_) {
        is_average = is_average_;
        ctl.write_bit<reg::avg0, 0>(is_average);
        ctl.write_bit<reg::avg1, 0>(is_average);
    }

    uint32_t get_num_average(uint32_t channel) {
        num_average = sts.read_reg(N_AVG_OFFSET[channel]);
        return num_average;
    }

    void set_num_average_min(uint32_t num_average_min_) {
        num_average_min = (num_average_min_ < 2) ? 0 : num_average_min_-2;
        ctl.write<reg::n_avg_min0>(num_average_min);
        ctl.write<reg::n_avg_min1>(num_average_min);
    }

    // Read channels and take one point every decim_factor points
    std::vector<float>& get_decimated_data(uint32_t decim_factor, uint32_t index_low, uint32_t index_high) {
        // Sanity checks
        if (index_high <= index_low || index_high > WFM_SIZE) {
            decimated_data.resize(0);
            return decimated_data;
        }

        ctl.set_bit<reg::addr, 1>();
        uint32_t n_pts = (index_high - index_low)/decim_factor;
        decimated_data.resize(2*n_pts);
        _wait_for_acquisition();

        is_average = sts.read_bit<reg::avg_on_out0, 0>();
        if (is_average) {
            float num_average_ = float(get_num_average(0));
            for (unsigned int i=0; i<n_pts; i++) {
                decimated_data[i] = float(raw_data[0][index_low + decim_factor * i]) / num_average_;
                decimated_data[i + n_pts] = float(raw_data[1][index_low + decim_factor * i]) / num_average_;
            }
        } else {
            for (unsigned int i=0; i<n_pts; i++) {
                decimated_data[i] = float(raw_data[0][index_low + decim_factor * i]);
                decimated_data[i + n_pts] = float(raw_data[1][index_low + decim_factor * i]);
            }
        }
        ctl.clear_bit<reg::addr, 1>();
        return decimated_data;
    }

  private:
    int32_t *raw_data[2] = {nullptr, nullptr};

    bool is_average;
    uint32_t num_average_min;
    uint32_t num_average;

    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::adc>& adc_map;

    // Acquired data buffers
    std::vector<float> decimated_data;

    // Internal functions
    void _wait_for_acquisition()
    {
        using namespace std::chrono_literals;
        auto begin = std::chrono::high_resolution_clock::now();

        do {
            if (num_average_min > 0) {
                auto now = std::chrono::high_resolution_clock::now();
                auto remain_wait = ctl.read<reg::n_avg_min0>() * wfm_time - (now - begin);

                // If acquisition time is larger than 1 ms, we sleep for the
                // typical overhead time to put the thread in sleep (~ 100 us).

                if (remain_wait > 1ms)
                    std::this_thread::sleep_for(remain_wait / 10);
            }
        } while (sts.read<reg::avg_ready0>() == 0
                || sts.read<reg::avg_ready1>() == 0);
    }

    void set_dac_periods(uint32_t dac_period0, uint32_t dac_period1) {
        ctl.write<reg::dac_period0>(dac_period0 - 1);
        ctl.write<reg::dac_period1>(dac_period1 - 1);
        reset();
    }

    void set_average_period(uint32_t average_period) {
        ctl.write<reg::avg_period>(average_period - 1);
        ctl.write<reg::avg_threshold>(average_period - 6);
        reset();
    }

};

#endif // __DRIVERS_OSCILLO_HPP__
