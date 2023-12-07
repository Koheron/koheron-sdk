/// FFT driver
///
/// (c) Koheron

#ifndef __DRIVERS_FFT_HPP__
#define __DRIVERS_FFT_HPP__

#include <context.hpp>

#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <cmath>
#include <limits>
#include <array>

#include <boards/alpha250/drivers/clock-generator.hpp>

class FFT
{
  public:
    FFT(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , ps_ctl(ctx.mm.get<mem::ps_control>())
    , ps_sts(ctx.mm.get<mem::ps_status>())
    , psd_map(ctx.mm.get<mem::psd>())
    , demod_map(ctx.mm.get<mem::demod>())
    , clk_gen(ctx.get<ClockGenerator>())
    {
        set_input_channel(0);
        set_scale_sch(0);
        set_fft_window(1);
        ctl.set_bit<reg::psd_valid, 0>();
        start_psd_acquisition();
    }

    //////////////////////////////////////
    // Power Spectral Density
    //////////////////////////////////////

    void set_input_channel(uint32_t channel) {
        if (channel >= 8) {
            ctx.log<ERROR>("FFT::set_input_channel invalid channel\n");
            return;
        }

        input_channel = channel;
        ctl.write<reg::channel_select>(channel);

    }

    void set_scale_sch(uint32_t scale_sch) {
        // LSB at 1 for forward FFT
        ps_ctl.write<reg::ctl_fft>(1 + (scale_sch << 1));
    }

    void set_fft_window(uint32_t window_id) {
        constexpr std::array<std::array<double, 6>, 4> window_coeffs = {{
            {1.0, 0, 0, 0, 0, 1.0},                      // Rectangular
            {0.5, 0.5, 0, 0, 0, 1.0},                    // Hann
            {1.0, 1.93, 1.29, 0.388, 0.028, 0.2},        // Flat top
            {0.35875, 0.48829, 0.14128, 0.01168, 0, 1.0} // Blackman-Harris
        }};

        if (window_id >= 4) {
            ctx.log<ERROR>("Invalid FFT window index \n");
            return;
        }

        set_cosine_sum_window(window_coeffs[window_id]);
        set_window_buffer();
        window_index = window_id;
    }

    // Read averaged spectrum data
    const auto& read_psd_raw() {
        std::lock_guard<std::mutex> lock(mutex);
        return psd_buffer_raw;
    }

    // Return the PSD in W/Hz
    const auto& read_psd() {
        std::lock_guard<std::mutex> lock(mutex);
        return psd_buffer;
    }

    uint32_t get_number_averages() const {
        return prm::n_cycles;
    }

    uint32_t get_fft_size() const {
        return prm::fft_size;
    }

    // Return the raw input value of each ADC channel
    // n_avg: number of averages
    const std::array<int32_t, prm::n_adc> get_adc_raw_data(uint32_t n_avg) {
        if (n_avg <= 1) {
            return { sts.read<reg::adc0, int16_t>(),
                     sts.read<reg::adc1, int16_t>() };
        } else {
            int32_t adc0 = 0;
            int32_t adc1 = 0;

            for (size_t i=0; i<n_avg; i++) {
                adc0 += sts.read<reg::adc0, int16_t>();
                adc1 += sts.read<reg::adc1, int16_t>();
            }

            return { int32_t(std::round(adc0 / double(n_avg))),
                     int32_t(std::round(adc1 / double(n_avg))) };
        }
    }

    const auto& get_window_index() const {
        return window_index;
    }

 private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::ps_control>& ps_ctl;
    Memory<mem::ps_status>& ps_sts;
    Memory<mem::psd>& psd_map;
    Memory<mem::demod>& demod_map;
    ClockGenerator& clk_gen;

    double fs_adc; // ADC sampling rate (Hz)
    std::array<std::array<float, prm::fft_size/2>, 2> freq_calibration; // Conversion to W/Hz

    std::array<double, prm::fft_size> window;
    double W1, W2; // Window correction factors
    uint32_t window_index;

    uint32_t input_channel = 0;
    std::array<double, 2> dds_freq = {{0.0, 0.0}};

    std::array<float, prm::fft_size/2> psd_buffer;
    std::array<float, prm::fft_size/2> psd_buffer_raw;
    std::thread psd_thread;
    std::mutex mutex;
    std::atomic<bool> psd_acquisition_started{false};
    std::atomic<uint32_t> acq_cycle_index{0};
    void psd_acquisition_thread();
    void start_psd_acquisition();

    // https://en.wikipedia.org/wiki/Window_function
    void set_cosine_sum_window(const std::array<double, 6>& a) {
        double sign;

        for (size_t i=0; i<prm::fft_size; i++) {
            window[i] = 0;

            for (size_t j=0; j<(a.size() - 1); j++) {
                j % 2 == 0 ? sign = 1.0 : sign = -1.0;
                window[i] += sign * a[j] * std::cos(2 * M_PI * i * j / double(prm::fft_size - 1));
            }

            window[i] *= a[a.size() - 1]; // Scaling
        }
    }

    // Vectors to convert PSD raw data into W/Hz
    void set_conversion_vectors() {
        constexpr double load = 50.0; // Ohm

        fs_adc = 15E6;;

        std::array<double, 2> vin = {8.0,8.0};

        float C0 =  (vin[0] / (2 << 20)) * (vin[0] / (2 << 20)) / prm::n_cycles / fs_adc / load / W2;
        float C1 =  (vin[1] / (2 << 20)) * (vin[1] / (2 << 20)) / prm::n_cycles / fs_adc / load / W2;

        for (unsigned int i=0; i<prm::fft_size/2; i++) {
            freq_calibration[0][i] = C0;
            freq_calibration[1][i] = C1;
        }
    }

    void set_window_buffer() {
        std::array<uint32_t, prm::fft_size> window_buffer;
        double res1 = 0;
        double res2 = 0;

        for (size_t i=0; i<prm::fft_size; i++) {
            window_buffer[i] = ((int32_t(32768 * window[i]) + 32768) % 65536) + 32768;
            res1 += window[i];
            res2 += window[i] * window[i];
        }

        demod_map.write_array(window_buffer);

        W1 = (res1 / prm::fft_size) * (res1 / prm::fft_size);
        W2 = res2 / prm::fft_size;
        set_conversion_vectors();
    }

    uint32_t get_cycle_index() {
        return ps_sts.read<reg::cycle_index>();
    }

}; // class FFT

inline void FFT::start_psd_acquisition() {
    if (! psd_acquisition_started) {
        psd_buffer.fill(0);
        psd_thread = std::thread{&FFT::psd_acquisition_thread, this};
        psd_thread.detach();
    }
}

inline void  FFT::psd_acquisition_thread() {
    using namespace std::chrono_literals;

    psd_acquisition_started = true;

    while (psd_acquisition_started) {
        uint32_t cycle_index = get_cycle_index();
        uint32_t previous_cycle_index = cycle_index;

        // Wait for data
        while (cycle_index >= previous_cycle_index) {
            auto sleep_time = std::chrono::nanoseconds((prm::n_cycles - cycle_index) * 8192 * 4);
            if (sleep_time > 1ms) {
                std::this_thread::sleep_for(sleep_time);
            }
            previous_cycle_index = cycle_index;
            cycle_index = get_cycle_index();
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            psd_buffer = psd_map.read_array<float, prm::fft_size/2, 0>();

            for (unsigned int i=0; i<prm::fft_size/2; i++) {
                psd_buffer[i] = psd_buffer_raw[i] * freq_calibration[input_channel][i];
            }
        }

        acq_cycle_index = get_cycle_index();
    }
}

#endif // __DRIVERS_FFT_HPP__