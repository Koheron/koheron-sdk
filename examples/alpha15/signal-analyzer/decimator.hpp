/// Pulse driver
///
/// (c) Koheron

#ifndef __ALPHA15_SIGNAL_ANALYZER_DECIMATOR_HPP__
#define __ALPHA15_SIGNAL_ANALYZER_DECIMATOR_HPP__

#include "server/drivers/fifo.hpp"
#include "./moving_averager.hpp"

#include <array>
#include <atomic>
#include <cstdint>
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
    void set_cic_rate(uint32_t fifo_idx, uint32_t rate);

    auto get_control_parameters() {
        return std::tuple{
            fs[0],
            fs[1],
            fifo_transfer_duration[0],
            fifo_transfer_duration[1],
            cic_rate[0],
            cic_rate[1],
            n_pts
        };
    }

    auto read_adc() const {
        return adc_data;
    }

    auto spectral_density() const {
        std::lock_guard lock(mutex[0]);
        return std::get<0>(psd);
    }

    auto spectral_density_lf() const {
        std::lock_guard lock(mutex[1]);
        return std::get<1>(psd);
    }

  private:
    static constexpr uint32_t fifo_depth = 16384;
    static constexpr uint32_t n_acq_max = fifo_depth / 2;
    static constexpr uint32_t n_fifo = 2 * fifo_depth; // Number of points read from FIFO
    static constexpr uint32_t n_segs = 4;
    static constexpr uint32_t n_pts = n_fifo / n_segs;

    Context& ctx;
    Fifo<mem::adc_fifo0> fifo0;
    Fifo<mem::adc_fifo1> fifo1;

    std::array<uint32_t, 2> cic_rate;
    float fs_adc;
    std::array<float, 2> fs;
    std::array<float, 2> fifo_transfer_duration;

    std::array<double, n_fifo> adc_data;
    std::array<std::array<double, n_pts>, 2> seg_data;

    // Data acquisition thread
    std::array<std::thread, 2> acq_thread;
    mutable std::array<std::mutex, 2> mutex;
    std::array<std::atomic<bool>, 2> acquisition_started{false, false};

    // Spectrum analyzer
    std::array<scicpp::signal::Spectrum<double>, 2> spectrum;
    std::array<MovingAverager<16>, 2> averager;
    std::array<std::vector<double>, 2> psd;

    template <uint32_t fifo_idx> void acquisition_thread();
    template <uint32_t fifo_idx> void acquire_fifo(uint32_t ntps_pts_fifo);
    void start_acquisition();
}; // Decimator

#endif // __ALPHA15_SIGNAL_ANALYZER_DECIMATOR_HPP__
