#ifndef __ALPHA15_SIGNAL_ANALYZER_FIFO_SPECTRAL_ANALYZER_HPP__
#define __ALPHA15_SIGNAL_ANALYZER_FIFO_SPECTRAL_ANALYZER_HPP__

#include "./fft.hpp"
#include "./moving_averager.hpp"
#include "server/context/context.hpp"
#include "server/drivers/fifo.hpp"
#include "boards/alpha15/drivers/clock-generator.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <mutex>
#include <vector>

#include <scicpp/core.hpp>
#include <scicpp/signal.hpp>

namespace sci = scicpp;
namespace sig = scicpp::signal;
namespace win = scicpp::signal::windows;

template<class Cfg>
class FifoSpectralAnalyzer {
  public:
    explicit FifoSpectralAnalyzer(Context& ctx_)
    : ctx(ctx_)
    , fifo()
    {
        psd.resize(1 + Cfg::n_pts / 2);
        set_cic_rate();
    };

    template<win::Window window>
    void set_window() {
        spectrum.window(window, Cfg::n_pts);
    }

    auto spectral_density() const {
        std::lock_guard lock(mutex);
        return psd;
    }

    void start_acquisition() {
        bool expected = false;
        if (acquisition_started.compare_exchange_strong(expected, true)) {
            acq_thread = std::thread(&FifoSpectralAnalyzer::acquisition_thread, this);
            acq_thread.detach();
        }
    }

    float fs;
    float fifo_transfer_duration;

  private:
    Context& ctx;
    Fifo<Cfg::fifo_mem> fifo;
    std::array<double, Cfg::n_pts> seg_data;
    uint32_t seg_cnt = 0;

    // Data acquisition thread
    std::thread acq_thread;
    mutable std::mutex mutex;
    std::atomic<bool> acquisition_started{false};

    // Spectrum analyzer
    scicpp::signal::Spectrum<double> spectrum;
    MovingAverager<Cfg::navg> averager;
    std::vector<double> psd;

    void set_cic_rate() {
        static_assert(Cfg::cic_rate > prm::cic_decimation_rate_min &&
                      Cfg::cic_rate < prm::cic_decimation_rate_max);

        auto& ctl = ctx.mm.get<mem::ps_control>();
        Cfg::fifo_idx == 0 ? ctl.write<reg::cic_rate0>(Cfg::cic_rate)
                           : ctl.write<reg::cic_rate1>(Cfg::cic_rate);

        const float fs_adc = ctx.get<ClockGenerator>().get_adc_sampling_freq()[0];
        fs = fs_adc / (2.0f * Cfg::cic_rate); // Sampling frequency (factor of 2 because of FIR)
        spectrum.fs(fs);
        ctx.logf("FifoSpectralAnalyzer: Sampling frequency fs[{}] = {} Hz\n", Cfg::fifo_idx, fs);

        fifo_transfer_duration = Cfg::n_pts / fs;
        ctx.logf("FifoSpectralAnalyzer: FIFO {} transfer duration = {} s\n",
                 Cfg::fifo_idx, fifo_transfer_duration);
    }

    void acquire(uint32_t ntps_pts_fifo) {
        const double vrange = ctx.get<FFT>().input_voltage_range();
        constexpr double nmax = 262144.0; // 2^18

        fifo.wait_for_data(ntps_pts_fifo, fs);

        for (uint32_t i = 0; i < ntps_pts_fifo; i++) {
            seg_data[seg_cnt] = vrange * static_cast<int32_t>(fifo.read()) / nmax / 4096.0;;
            ++seg_cnt;

            if (seg_cnt == Cfg::n_pts) {
                seg_cnt = 0;
                std::lock_guard lock(mutex);
                averager.append(spectrum.periodogram<sig::DENSITY, false>(seg_data));

                if (averager.full()) {
                    psd = averager.average();
                }
            }
        }
    }

    void acquisition_thread() {
        acquisition_started.store(true, std::memory_order_release);
        while (acquisition_started.load(std::memory_order_acquire)) {
            if constexpr (Cfg::n_fifo <= Cfg::n_acq_max) {
                acquire(Cfg::n_fifo);
            } else {
                uint32_t n_remaining = Cfg::n_fifo;
                while (n_remaining) {
                    const uint32_t chunk = (n_remaining >= Cfg::n_acq_max) ? Cfg::n_acq_max : n_remaining;
                    acquire(chunk);
                    n_remaining -= chunk;
                }
            }
        }
    }
};

#endif // __ALPHA15_SIGNAL_ANALYZER_FIFO_SPECTRAL_ANALYZER_HPP__