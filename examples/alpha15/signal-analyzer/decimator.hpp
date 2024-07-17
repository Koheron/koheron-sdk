/// Pulse driver
///
/// (c) Koheron

#ifndef __DRIVERS_DECIMATOR_HPP__
#define __DRIVERS_DECIMATOR_HPP__

#include <context.hpp>

#include <atomic>
#include <thread>
#include <mutex>

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
    , fifo(ctx)
    , clk_gen(ctx.get<ClockGenerator>())
    {
        fs_adc = clk_gen.get_adc_sampling_freq()[0];
        // set_cic_rate(prm::cic_decimation_rate_default);
        set_cic_rate(64);
        psd.resize(1 + n_pts / 2);
        spectrum.window(win::Hann, n_pts);
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
        fifo_transfer_duration = n_pts / fs;
        ctx.log<INFO>("Decimator: FIFO transfer duration = %f s\n",
                      double(fifo_transfer_duration));
        spectrum.fs(fs);
        ctl.write<reg::cic_rate>(cic_rate);
    }

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
    // static constexpr uint32_t n_pts = 8192;
    static constexpr uint32_t n_pts = 16384;

    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Fifo<mem::adc_fifo> fifo;

    ClockGenerator& clk_gen;

    uint32_t cic_rate;
    float fs_adc, fs;
    float fifo_transfer_duration;

    std::array<double, n_pts> adc_data;

    // Data acquisition thread
    std::thread acq_thread;
    std::mutex mutex;
    std::atomic<bool> acquisition_started{false};
    void acquisition_thread();
    void start_acquisition();

    // Spectrum analyzer
    sig::Spectrum<double> spectrum;
    std::vector<double> psd;
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
        fifo.wait_for_data(n_pts, fs);

        {
            std::lock_guard<std::mutex> lock(mutex);

            const double vrange = 2.048; // TODO Use calibration and range
            constexpr double nmax = 262144.0; // 2^18

            for (uint32_t i = 0; i < n_pts; i++) {
                adc_data[i] = vrange * static_cast<int32_t>(fifo.read()) / nmax / 4096.0;
            }

            psd = spectrum.periodogram<sig::DENSITY, false>(adc_data);
        }
    }
}

#endif // __DRIVERS_DECIMATOR_HPP__
