/// Pulse driver
///
/// (c) Koheron

#ifndef __DRIVERS_DECIMATOR_HPP__
#define __DRIVERS_DECIMATOR_HPP__

#include <context.hpp>

#include <array>
#include <atomic>
#include <thread>
#include <mutex>

#include <scicpp/core.hpp>
#include <scicpp/signal.hpp>

#include <server/drivers/fifo.hpp>
#include <boards/alpha15/drivers/clock-generator.hpp>

namespace {
    namespace sci = scicpp;
    namespace sig = scicpp::signal;
    namespace win = scicpp::signal::windows;
}

class Decimator
{
  public:
    Decimator(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , ps_ctl(ctx.mm.get<mem::ps_control>())
    , fifo(ctx)
    , clk_gen(ctx.get<ClockGenerator>())
    {
        fs_adc = clk_gen.get_adc_sampling_freq()[0];
        // set_cic_rate(prm::cic_decimation_rate_default);
        set_cic_rate(32);
        psd.resize(1 + n_pts / 2);
        psd_full.resize(1 + n_pts_full / 2);
        ctx.log<INFO>("psd_full.size = %u", psd_full.size());
        spectrum.window(win::Hann, n_pts);
        spectrum_full.window(win::Hann, n_pts_full);
        start_acquisition();
    }

    void set_cic_rate(uint32_t rate) {
        if (rate < prm::cic_decimation_rate_min ||
            rate > prm::cic_decimation_rate_max) {
            ctx.log<ERROR>("Decimator: CIC rate out of range\n");
            return;
        }

        cic_rate = rate;
        fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
        ctx.log<INFO>("Decimator: Sampling frequency = %f Hz\n", double(fs));
        fifo_transfer_duration = n_pts / fs;
        ctx.log<INFO>("Decimator: FIFO transfer duration = %f s\n",
                      double(fifo_transfer_duration));
        spectrum.fs(fs);
        spectrum_full.fs(fs / n_decim);
        ps_ctl.write<reg::cic_rate>(cic_rate);
    }

    auto get_control_parameters() {
        return std::tuple{fs, fifo_transfer_duration, cic_rate, n_pts, n_pts_full, fs / n_decim};
    }

    auto read_adc() const {
        return adc_data;
    }

    auto spectral_density() const {
        return psd;
    }

    auto spectral_density_full() const {
        return psd_full;
    }

  private:
    static constexpr uint32_t fifo_depth = 16384;
    static constexpr uint32_t n_fifo = 2 * fifo_depth; // Number of points read from FIFO
    static constexpr uint32_t n_decim = 32;
    static constexpr uint32_t n_pts_full = n_fifo / n_decim;
    static constexpr uint32_t n_segs = 16;
    static constexpr uint32_t n_pts = n_fifo / n_segs;

    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::ps_control>& ps_ctl;
    Fifo<mem::adc_fifo> fifo;

    ClockGenerator& clk_gen;

    uint32_t cic_rate;
    float fs_adc, fs;
    float fifo_transfer_duration;

    std::array<double, n_fifo> adc_data;
    std::array<double, n_pts> seg_data;

    // Data acquisition thread
    std::thread acq_thread;
    std::mutex mutex;
    std::atomic<bool> acquisition_started{false};
    void acquisition_thread();
    void start_acquisition();
    void acquire_fifo(uint32_t ntps_pts_fifo);

    // Spectrum analyzers
    sig::Spectrum<double> spectrum;
    std::vector<double> psd;

    sig::Spectrum<double> spectrum_full;
    std::vector<double> psd_full; // PSD full FIFO buffer
}; // Decimator

inline void Decimator::start_acquisition() {
    if (! acquisition_started) {
        adc_data.fill(0);
        acq_thread = std::thread{&Decimator::acquisition_thread, this};
        acq_thread.detach();
    }
}

inline void Decimator::acquisition_thread() {
    acquisition_started = true;

    while (acquisition_started) {
        if (n_fifo <= fifo_depth) {
            acquire_fifo(n_fifo);
        } else {
            uint32_t n_read = 0;

            while (n_read < n_fifo) {
                acquire_fifo(fifo_depth);
                n_read += fifo_depth;
            }
        }
    }
}

inline void Decimator::acquire_fifo(uint32_t ntps_pts_fifo) {
    using namespace sci::operators;

    uint32_t seg_cnt = 0;
    // uint32_t adc_data_cnt = 0;
    // double value_avg = 0.0;

    fifo.wait_for_data(ntps_pts_fifo, fs);
    const double vrange = 2.048; // TODO Use calibration and range
    constexpr double nmax = 262144.0; // 2^18

    for (uint32_t i = 0; i < ntps_pts_fifo; i++) {
        const auto value = vrange * static_cast<int32_t>(fifo.read()) / nmax / 4096.0;
        // value_avg += value;
        // value_avg += 0.5 * (value - value_avg);
        
        // // Bad decimation => Aliasing
        // if (i % n_decim) {
        //     adc_data[adc_data_cnt] = value_avg;
        //     // adc_data[adc_data_cnt] = value_avg / double(n_decim);
        //     // adc_data[adc_data_cnt] = value;
        //     ++adc_data_cnt;

        //     if (adc_data_cnt >= n_pts_full) {
        //         adc_data_cnt = n_pts_full - 1;
        //     }

        //     // value_avg = 0.0;
        // }

        adc_data[i] = value;

        seg_data[seg_cnt] = value;
        ++seg_cnt;

        if (seg_cnt == n_pts) {
            seg_cnt = 0;

            {
                std::lock_guard<std::mutex> lock(mutex);
                // Exponential moving average (IIR filter)
                // TODO FIR moving average with circular buffer
                psd = psd + 0.3 * (spectrum.periodogram<sig::DENSITY, false>(seg_data) - psd);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        // psd_full = double(n_decim) * spectrum_full.periodogram<sig::DENSITY, false>(adc_data);

        // low-pass filter
        //cutoff=0.45
        // const auto taps = std::array{
        //     -9.18467962e-05, 2.96162089e-04, 5.45391695e-03, 1.29189563e-03,
        //     -3.73843874e-02,-3.18846128e-02, 1.55524774e-01, 4.06794099e-01,
        //     4.06794099e-01, 1.55524774e-01,-3.18846128e-02, -3.73843874e-02,
        //     1.29189563e-03, 5.45391695e-03, 2.96162089e-04, -9.18467962e-05};

        // cutoff=0.1
        const auto taps = std::array{
            1.18100557e-04, 1.92827202e-03, 9.10736522e-03, 2.67254841e-02,
            5.78178521e-02, 9.91440603e-02, 1.39819544e-01, 1.65339322e-01,
            1.65339322e-01, 1.39819544e-01, 9.91440603e-02, 5.78178521e-02,
            2.67254841e-02, 9.10736522e-03, 1.92827202e-03, 1.18100557e-04};

        auto data_filt = sig::convolve(adc_data, taps);

        // ctx.log<INFO>("adc_data[10] = %lf", adc_data[10]);
        // ctx.log<INFO>("data_filt[10] = %lf", data_filt[10]);

        // Decimation
        std::array<double, n_pts_full> full_data;

        uint32_t cnt = 0;
        double value = 0.0;

        for (uint32_t i = 0; i < data_filt.size(); i++) {
            value += data_filt[i];
    
            if (i % n_decim) {
                full_data[cnt] = value / double(n_decim);
                ++cnt;

                if (cnt >= n_pts_full) {
                    cnt = n_pts_full - 1;
                }

                value = 0.0;
            }
        }

        // ctx.log<INFO>("full_data[10] = %lf", full_data[10]);

        psd_full = double(n_decim) * spectrum_full.periodogram<sig::DENSITY, false>(full_data);
        // ctx.log<INFO>("psd_full[10] = %lf", psd_full[10]);

        
        // ctx.log<INFO>("psd_full.size = %u", psd_full.size());
        // ctx.log<INFO>("data_filt.size = %u", data_filt.size());
    }
}

#endif // __DRIVERS_DECIMATOR_HPP__
