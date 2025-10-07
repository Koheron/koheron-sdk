/// FFT driver
///
/// (c) Koheron

#ifndef __ALPHA250_FFT_FFT_HPP__
#define __ALPHA250_FFT_FFT_HPP__

#include "server/context/context.hpp"

#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <array>

class ClockGenerator;

class FFT
{
  public:
    FFT(Context& ctx_);
    void set_input_channel(uint32_t channel);
    void set_scale_sch(uint32_t scale_sch);
    void set_fft_window(uint32_t window_id);

    // Read averaged spectrum data
    const auto& read_psd_raw() {
        std::lock_guard<std::mutex> lock(mutex);
        return psd_buffer_raw;
    }

    // Return the PSD in W/Hz
    const auto& read_psd() {
        std::lock_guard<std::mutex> lock(mutex);
        return psd_buffer;
    }

    uint32_t get_number_averages() const {
        return prm::n_cycles;
    }

    uint32_t get_fft_size() const {
        return prm::fft_size;
    }

    // Return the raw input value of each ADC channel
    // n_avg: number of averages
    std::array<int32_t, prm::n_adc> get_adc_raw_data(uint32_t n_avg);

    void set_dds_freq(uint32_t channel, double freq_hz);

    auto get_control_parameters() {
        return std::make_tuple(dds_freq[0], dds_freq[1], fs_adc, input_channel, W1, W2);
    }

    const auto& get_window_index() const {
        return window_index;
    }

 private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::psd>& psd_map;
    Memory<mem::demod>& demod_map;
    ClockGenerator& clk_gen;

    double fs_adc; // ADC sampling rate (Hz)
    std::array<std::array<float, prm::fft_size/2>, 2> freq_calibration; // Conversion to W/Hz

    double S1, S2, W1, W2, ENBW; // Window correction factors
    uint32_t window_index;

    uint32_t input_channel = 0;
    std::array<double, 2> dds_freq = {{0.0, 0.0}};

    std::array<float, prm::fft_size/2> psd_buffer_raw;
    std::array<float, prm::fft_size/2> psd_buffer;
    std::thread psd_thread;
    std::mutex mutex;
    std::atomic<bool> psd_acquisition_started{false};
    std::atomic<uint32_t> acq_cycle_index{0};
    void psd_acquisition_thread();
    void start_psd_acquisition();

    // Vectors to convert PSD raw data into W/Hz
    void set_conversion_vectors();
    void set_window(const std::array<double, prm::fft_size> &window);
    uint32_t get_cycle_index();
}; // class FFT

#endif // __ALPHA250_FFT_FFT_HPP__
