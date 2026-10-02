#pragma once

#include "fft-board.hpp"
#include "server/drivers/fft/core.hpp"
#include <tuple>

// Keep the public method order and response shapes stable for existing clients.
class FFT {
 public:
    FFT() = default;
    void set_input_channel(uint32_t channel) { core.set_input_channel(channel); }
    void set_scale_sch(uint32_t scale_sch) { core.set_scale_sch(scale_sch); }
    void set_fft_window(uint32_t window_id) { core.set_fft_window(window_id); }
    auto read_psd_raw() { return core.read_psd_raw(); }
    auto read_psd() { return core.read_psd(); }
    uint32_t get_number_averages() const { return prm::n_cycles; }
    uint32_t get_fft_size() const { return prm::fft_size; }
    std::array<int32_t, prm::n_adc> get_adc_raw_data(uint32_t n_avg) {
        return core.get_adc_raw_data(n_avg);
    }
    void set_dds_freq(uint32_t channel, double freq_hz) { core.set_dds_freq(channel, freq_hz); }
    auto get_control_parameters() {
        const auto s = core.get_control_state();
        return std::tuple{s.dds[0], s.dds[1], s.sample_rate, s.channel, s.w1, s.w2};
    }
    auto get_window_index() { return core.get_control_state().window; }

 private:
    fft::Core<RedPitayaFftBoard> core;
};
