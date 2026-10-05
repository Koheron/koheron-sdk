#ifndef __ALPHA15_SIGNAL_ANALYZER_FIFO_SPECTRAL_ANALYZER_HPP__
#define __ALPHA15_SIGNAL_ANALYZER_FIFO_SPECTRAL_ANALYZER_HPP__

#include "./fft.hpp"
#include "./moving_averager.hpp"
#include "./spectrum_snapshot.hpp"

#include "server/runtime/driver_manager.hpp"
#include "server/runtime/syslog.hpp"
#include "server/hardware/memory_manager.hpp"
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
    explicit FifoSpectralAnalyzer()
    : fifo()
    {
        psd.resize(1 + Cfg::n_pts / 2);
        set_cic_rate();
    };

    template<win::Window window>
    void set_window() {
        std::lock_guard lock(mutex);
        spectrum.window(window, Cfg::n_pts);
        restart_locked();
    }

    auto spectral_density() const {
        std::lock_guard lock(mutex);
        return psd;
    }

    uint32_t restart_acquisition() {
        std::lock_guard lock(mutex);
        return restart_locked();
    }

    auto get_spectrum_snapshot() const {
        std::lock_guard lock(mutex);
        return publication.snapshot();
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
    SpectrumSnapshot publication{1 + Cfg::n_pts / 2};
    bool reset_pending = true;
    uint32_t discard_remaining = 0;

    uint32_t restart_locked() {
        averager.clear();
        std::fill(psd.begin(), psd.end(), std::numeric_limits<double>::quiet_NaN());
        reset_pending = true;
        return publication.restart();
    }

    void set_cic_rate() {
        static_assert(Cfg::cic_rate > prm::cic_decimation_rate_min &&
                      Cfg::cic_rate < prm::cic_decimation_rate_max);

        auto& ctl = hw::get_memory<mem::ps_control>();

        if constexpr (Cfg::fifo_idx == 0) {
            ctl.write<reg::cic_rate0>(Cfg::cic_rate);
        } else {
            ctl.write<reg::cic_rate1>(Cfg::cic_rate);
        }

        const float fs_adc = rt::get_driver<ClockGenerator>().get_adc_sampling_freq()[0];
        fs = fs_adc / (2.0f * Cfg::cic_rate); // Sampling frequency (factor of 2 because of FIR)
        spectrum.fs(fs);
        logf("FifoSpectralAnalyzer: Sampling frequency fs[{}] = {} Hz\n", Cfg::fifo_idx, fs);

        fifo_transfer_duration = Cfg::n_pts / fs;
        logf("FifoSpectralAnalyzer: FIFO {} transfer duration = {} s\n",
             Cfg::fifo_idx, fifo_transfer_duration);
    }

    void acquire(uint32_t ntps_pts_fifo) {
        constexpr double nmax = 262144.0; // 2^18

        fifo.wait_for_data(ntps_pts_fifo, fs);

        std::lock_guard lock(mutex);
        if (reset_pending) {
            seg_cnt = 0;
            // Drain queued FIFO samples and a full segment of CIC/FIR settling.
            // Only the acquisition thread touches the FIFO and segment state.
            discard_remaining = Cfg::n_fifo + Cfg::n_pts;
            reset_pending = false;
        }
        const double vrange = rt::get_driver<FFT>().input_voltage_range();

        for (uint32_t i = 0; i < ntps_pts_fifo; i++) {
            const auto sample = static_cast<int32_t>(fifo.read());
            if (discard_remaining) { --discard_remaining; continue; }
            seg_data[seg_cnt] = vrange * sample / nmax / 4096.0;
            ++seg_cnt;

            if (seg_cnt == Cfg::n_pts) {
                seg_cnt = 0;
                averager.append(spectrum.periodogram<sig::SpectrumScaling::DENSITY, false>(seg_data));

                if (averager.full()) {
                    psd = averager.average();
                    publication.publish(psd);
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
