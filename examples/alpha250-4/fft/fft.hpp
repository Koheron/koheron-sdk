/// FFT driver
///
/// (c) Koheron

#ifndef __DRIVERS_FFT_HPP__
#define __DRIVERS_FFT_HPP__

#include "server/hardware/memory_manager.hpp"

#include <chrono>
#include <cmath>
#include <limits>

#include <array>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>

class ClockGenerator;
class Ltc2157;

class FFT
{
  public:
    FFT();

    void set_input_channel(uint32_t channel);
    void set_scale_sch(uint32_t scale_sch);
    void set_fft_window(uint32_t window_id);
    std::array<float, prm::fft_size/2> read_psd_raw(uint32_t adc); // Read averaged spectrum data
    std::array<float, prm::fft_size/2> read_psd(uint32_t adc);     // Return the PSD in W/Hz

    uint32_t get_number_averages() const {
        return prm::n_cycles;
    }

    uint32_t get_fft_size() const {
        return prm::fft_size;
    }

    auto get_acq_cycle_index(uint32_t adc) {
        return acq_cycle_index[adc].load();
    }

    // Return the raw input value of each ADC channel
    // n_avg: number of averages
    std::array<int32_t, 2 * prm::n_adc> get_adc_raw_data(uint32_t n_avg);

    auto get_control_parameters() {
        return std::tuple{
            fs_adc[0],
            fs_adc[1],
            input_channel,
            W1,
            W2
        };
    }

    auto get_window_index() const {
        return window_index;
    }

 private:
    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::status>& sts;
    ClockGenerator& clk_gen;
    Ltc2157& ltc2157;

    std::array<double, 2> fs_adc{}; // ADC sampling rates (Hz)
    std::array<std::array<float, prm::fft_size/2>, 4> freq_calibration{}; // Conversion to W/Hz

    // std::array<double, prm::fft_size> window{};
    double S1, S2, W1, W2, ENBW; // Window correction factors
    uint32_t window_index;

    uint32_t input_channel = 0;

    std::array<std::array<float, prm::fft_size/2>, 2> psd_buffer_raw{};
    std::array<std::array<float, prm::fft_size/2>, 2> psd_buffer{};
    std::array<std::thread, 2> psd_thread{};
    std::array<std::mutex, 2> mutex{};
    std::array<std::atomic<bool>, 2> psd_acquisition_started{};
    std::array<std::atomic<uint32_t>, 2> acq_cycle_index{};

    template <uint32_t adc> void psd_acquisition_thread();
    template <uint32_t adc> void start_psd_acquisition();

    // Vectors to convert PSD raw data into W/Hz
    void set_conversion_vectors();
    void set_window(const std::array<double, prm::fft_size> &window);
    uint32_t get_cycle_index(uint32_t adc);
}; // class FFT

#endif // __DRIVERS_FFT_HPP__
