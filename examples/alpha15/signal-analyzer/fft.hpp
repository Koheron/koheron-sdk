/// FFT driver
///
/// (c) Koheron

#ifndef __ALPHA15_SIGNAL_ANALYZER_FFT_HPP__
#define __ALPHA15_SIGNAL_ANALYZER_FFT_HPP__

#include "server/context/memory_manager.hpp"

#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <tuple>

namespace koheron { class DriverManager; }

class FFT
{
  public:
    FFT();
    void set_offsets(uint32_t off0, uint32_t off1);
    void select_adc_channel(uint32_t channel);
    void set_operation(uint32_t operation);
    void set_scale_sch(uint32_t scale_sch);
    void set_fft_window(uint32_t window_id);

    // Return the PSD in W/Hz
    std::array<float, prm::fft_size/2> read_psd();

    uint32_t get_number_averages() const {
        return prm::n_cycles;
    }

    uint32_t get_fft_size() const {
        return prm::fft_size;
    }

    auto get_window_index() const {
        return window_index;
    }

    auto get_control_parameters() {
        return std::tuple{fs_adc, input_channel, input_operation, S1, S2, ENBW};
    }

    double input_voltage_range();

 private:
    koheron::DriverManager& dm;
    MemoryManager& mm;
    Memory<mem::control>& ctl;

    double fs_adc; // ADC sampling rate (Hz)

    double S1, S2, W1, W2, ENBW; // Window correction factors
    uint32_t window_index;

    uint32_t input_channel = 0;
    uint32_t input_operation = 0;

    std::array<float, prm::fft_size/2> psd_buffer;
    std::thread psd_thread;
    std::mutex mutex;
    std::atomic<bool> psd_acquisition_started{false};
    std::atomic<uint32_t> acq_cycle_index{0};

    void psd_acquisition_thread();
    void start_psd_acquisition();
    uint32_t input_range();
    float calibration(); // Factor to convert PSD raw data into V^2/Hz
    void set_window(const std::array<double, prm::fft_size> &window);
    uint32_t get_cycle_index();
}; // class FFT

#endif // __ALPHA15_SIGNAL_ANALYZER_FFT_HPP__