/// Pulse driver
///
/// (c) Koheron

#ifndef __ALPHA15_SIGNAL_ANALYZER_DECIMATOR_HPP__
#define __ALPHA15_SIGNAL_ANALYZER_DECIMATOR_HPP__

#include "server/drivers/fifo.hpp"
#include "./moving_averager.hpp"

#include <array>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <scicpp/signal.hpp>

class Context;

class Decimator
{
  public:
    Decimator(Context& ctx_);
    void set_fft_window(uint32_t window_id);
    void set_cic_rate(uint32_t rate);

    auto get_control_parameters() {
        return std::tuple{fs, fifo_transfer_duration, cic_rate, n_pts};
    }

    auto read_adc() const {
        return adc_data;
    }

    auto spectral_density() const {
        return psd;
    }

  private:
    static constexpr uint32_t fifo_depth = 16384;
    static constexpr uint32_t n_fifo = 2 * fifo_depth; // Number of points read from FIFO
    static constexpr uint32_t n_segs = 4;
    static constexpr uint32_t n_pts = n_fifo / n_segs;

    Context& ctx;
    Fifo<mem::adc_fifo> fifo;

    uint32_t cic_rate;
    float fs_adc, fs;
    float fifo_transfer_duration;

    std::array<double, n_fifo> adc_data;
    std::array<double, n_pts> seg_data;

    // Data acquisition thread
    std::thread acq_thread;
    std::mutex mutex;
    std::atomic<bool> acquisition_started{false};

    // Spectrum analyzer
    scicpp::signal::Spectrum<double> spectrum;
    MovingAverager<16> averager;
    std::vector<double> psd;

    void acquisition_thread();
    void start_acquisition();
    void acquire_fifo(uint32_t ntps_pts_fifo);
}; // Decimator

#endif // __ALPHA15_SIGNAL_ANALYZER_DECIMATOR_HPP__
