#include "./decimator.hpp"
#include "./fft.hpp"

#include "server/context/context.hpp"
#include "boards/alpha15/drivers/clock-generator.hpp"

#include <scicpp/core.hpp>

namespace sci = scicpp;
namespace sig = scicpp::signal;
namespace win = scicpp::signal::windows;

Decimator::Decimator(Context& ctx_)
: ctx(ctx_)
, fifo0(ctx)
, fifo1(ctx)
{
    fs_adc = ctx.get<ClockGenerator>().get_adc_sampling_freq()[0];
    set_cic_rate(0, 16);
    set_cic_rate(1, 256);
    psd[0].resize(1 + n_pts / 2);
    psd[1].resize(1 + n_pts / 2);
    set_fft_window(1);
    start_acquisition();
}

void Decimator::set_fft_window(uint32_t window_id) {
    for (const int& idx : {0, 1}) {
        switch (window_id) {
        case 0:
            spectrum[idx].window(win::Boxcar, n_pts);
            break;
        case 1:
            spectrum[idx].window(win::Hann, n_pts);
            break;
        case 2:
            spectrum[idx].window(win::Flattop, n_pts);
            break;
        case 3:
            spectrum[idx].window(win::Blackmanharris, n_pts);
            break;
        default:
            ctx.log<ERROR>("Decimator: Invalid window index\n");
            return;
        }
    }
}

void Decimator::set_cic_rate(uint32_t fifo_idx, uint32_t rate) {
    if (rate < prm::cic_decimation_rate_min ||
        rate > prm::cic_decimation_rate_max) {
        ctx.log<ERROR>("Decimator: CIC rate out of range\n");
        return;
    }

    cic_rate[fifo_idx] = rate;
    auto& ctl = ctx.mm.get<mem::ps_control>();
    fifo_idx == 0 ? ctl.write<reg::cic_rate0>(rate)
                  : ctl.write<reg::cic_rate1>(rate);

    fs[fifo_idx] = fs_adc / (2.0f * cic_rate[fifo_idx]); // Sampling frequency (factor of 2 because of FIR)
    spectrum[fifo_idx].fs(fs[fifo_idx]);
    ctx.logf<INFO>("Decimator: Sampling frequencies fs[{}] = {} Hz\n", fifo_idx, fs[fifo_idx]);

    fifo_transfer_duration[fifo_idx] = n_pts / fs[fifo_idx];
    ctx.logf<INFO>("Decimator: FIFO {} transfer duration = {} s\n",
                   fifo_idx, fifo_transfer_duration[fifo_idx]);
}

template <uint32_t fifo_idx>
void Decimator::acquire_fifo(uint32_t ntps_pts_fifo) {
    using namespace sci::operators;

    uint32_t seg_cnt = 0;

    if constexpr (fifo_idx == 0) {
        fifo0.wait_for_data(ntps_pts_fifo, fs[0]);
    } else {
        fifo1.wait_for_data(ntps_pts_fifo, fs[1]);
    }

    const double vrange = ctx.get<FFT>().input_voltage_range();
    constexpr double nmax = 262144.0; // 2^18

    for (uint32_t i = 0; i < ntps_pts_fifo; i++) {
        double value = 0.0;

        if constexpr (fifo_idx == 0) {
            value = vrange * static_cast<int32_t>(fifo0.read()) / nmax / 4096.0;
            adc_data[i] = value;
        } else {
            value = vrange * static_cast<int32_t>(fifo1.read()) / nmax / 4096.0;
        }

        std::get<fifo_idx>(seg_data)[seg_cnt] = value;
        ++seg_cnt;

        if (seg_cnt == n_pts) {
            std::lock_guard lock(mutex[fifo_idx]);

            seg_cnt = 0;
            auto power_spectrum = std::get<fifo_idx>(spectrum).template periodogram<sig::DENSITY, false>(std::get<fifo_idx>(seg_data));
            std::get<fifo_idx>(averager).append(std::move(power_spectrum));

            if (std::get<fifo_idx>(averager).full()) {
                std::get<fifo_idx>(psd) = std::get<fifo_idx>(averager).average();
            }
        }
    }
}

void Decimator::start_acquisition() {
    if (! acquisition_started[0]) {
        acq_thread[0] = std::thread{&Decimator::acquisition_thread<0>, this};
        acq_thread[0].detach();
    }

    if (! acquisition_started[1]) {
        acq_thread[1] = std::thread{&Decimator::acquisition_thread<1>, this};
        acq_thread[1].detach();
    }
}

template <uint32_t fifo_idx>
void Decimator::acquisition_thread() {
    std::get<fifo_idx>(acquisition_started) = true;

    while (std::get<fifo_idx>(acquisition_started)) {
        if constexpr (n_fifo <= n_acq_max) {
            acquire_fifo<fifo_idx>(n_fifo);
        } else {
            uint32_t n_remaining = n_fifo;

            while (n_remaining > 0) {
                if (n_remaining >= n_acq_max) {
                    acquire_fifo<fifo_idx>(n_acq_max);
                    n_remaining -= n_acq_max;
                } else {
                    acquire_fifo<fifo_idx>(n_remaining);
                    n_remaining = 0;
                }
            }
        }
    }
}
