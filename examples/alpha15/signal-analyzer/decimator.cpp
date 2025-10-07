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
, fifo(ctx)
{
    fs_adc = ctx.get<ClockGenerator>().get_adc_sampling_freq()[0];
    set_cic_rate(32);
    psd.resize(1 + n_pts / 2);
    set_fft_window(1);
    start_acquisition();
}

void Decimator::set_fft_window(uint32_t window_id) {
    switch (window_id) {
        case 0:
        spectrum.window(win::Boxcar, n_pts);
        break;
        case 1:
        spectrum.window(win::Hann, n_pts);
        break;
        case 2:
        spectrum.window(win::Flattop, n_pts);
        break;
        case 3:
        spectrum.window(win::Blackmanharris, n_pts);
        break;
        default:
        ctx.log<ERROR>("Decimator: Invalid window index\n");
        return;
    }
}

void Decimator::set_cic_rate(uint32_t rate) {
    if (rate < prm::cic_decimation_rate_min ||
        rate > prm::cic_decimation_rate_max) {
        ctx.log<ERROR>("Decimator: CIC rate out of range\n");
        return;
    }

    cic_rate = rate;
    fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
    ctx.logf<INFO>("Decimator: Sampling frequency = {} Hz\n", fs);
    fifo_transfer_duration = n_pts / fs;
    ctx.logf<INFO>("Decimator: FIFO transfer duration = {} s\n", fifo_transfer_duration);
    spectrum.fs(fs);
    ctx.mm.get<mem::ps_control>().write<reg::cic_rate>(cic_rate);
}

void Decimator::acquire_fifo(uint32_t ntps_pts_fifo) {
    using namespace sci::operators;

    uint32_t seg_cnt = 0;

    fifo.wait_for_data(ntps_pts_fifo, fs);
    const double vrange = ctx.get<FFT>().input_voltage_range();
    constexpr double nmax = 262144.0; // 2^18

    for (uint32_t i = 0; i < ntps_pts_fifo; i++) {
        const auto value = vrange * static_cast<int32_t>(fifo.read()) / nmax / 4096.0;
        adc_data[i] = value;

        seg_data[seg_cnt] = value;
        ++seg_cnt;

        if (seg_cnt == n_pts) {
            seg_cnt = 0;

            {
                std::lock_guard<std::mutex> lock(mutex);
                averager.append(spectrum.periodogram<sig::DENSITY, false>(seg_data));

                if (averager.full()) {
                    psd = averager.average();
                }
            }
        }
    }
}

void Decimator::start_acquisition() {
    if (! acquisition_started) {
        acq_thread = std::thread{&Decimator::acquisition_thread, this};
        acq_thread.detach();
    }
}

void Decimator::acquisition_thread() {
    acquisition_started = true;

    while (acquisition_started) {
        if constexpr (n_fifo <= fifo_depth) {
            acquire_fifo(n_fifo);
        } else {
            uint32_t n_remaining = n_fifo;

            while (n_remaining > 0) {
                if (n_remaining >= fifo_depth) {
                    acquire_fifo(fifo_depth);
                    n_remaining -= fifo_depth;
                } else {
                    acquire_fifo(n_remaining);
                    n_remaining = 0;
                }
            }
        }
    }
}
