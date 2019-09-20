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
#include <boards/alpha250-4/drivers/ltc2157.hpp>

class FFT
{
  public:
    FFT(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , psd_map0(ctx.mm.get<mem::psd0>())
    , demod_map0(ctx.mm.get<mem::demod0>())
    , psd_map1(ctx.mm.get<mem::psd1>())
    , demod_map1(ctx.mm.get<mem::demod1>())
    , clk_gen(ctx.get<ClockGenerator>())
    , ltc2157(ctx.get<Ltc2157>())
    {
        set_input_channel(0);
        set_scale_sch(0);
        set_fft_window(1);
        ctl.set_bit<reg::psd_valid0, 0>();
        ctl.set_bit<reg::psd_valid1, 0>();
        start_psd_acquisition<0>();
        start_psd_acquisition<1>();
    }

    //////////////////////////////////////
    // Power Spectral Density
    //////////////////////////////////////

    void set_input_channel(uint32_t channel) {
        if (channel >= 2) {
            ctx.log<ERROR>("FFT::set_input_channel invalid channel\n");
            return;
        }

        input_channel = channel;
        ctl.write<reg::psd_input_sel0>(channel);
        ctl.write<reg::psd_input_sel1>(channel);
    }

    void set_scale_sch(uint32_t scale_sch) {
        // LSB at 1 for forward FFT
        ctl.write<reg::ctl_fft0>(1 + (scale_sch << 1));
        ctl.write<reg::ctl_fft1>(1 + (scale_sch << 1));
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
    auto read_psd_raw(uint32_t adc) {
        if (adc >= 2) {
            ctx.log<ERROR>("FFT::read_psd_raw: Invalid adc\n");
            return std::array<float, prm::fft_size/2>{};
        }

        std::lock_guard<std::mutex> lock(mutex);
        return psd_buffer_raw[adc];
    }

    // Return the PSD in W/Hz
    auto read_psd(uint32_t adc) {
        if (adc >= 2) {
            ctx.log<ERROR>("FFT::read_psd: Invalid adc\n");
            return std::array<float, prm::fft_size/2>{};
        }

        std::lock_guard<std::mutex> lock(mutex);
        return psd_buffer[adc];
    }

    uint32_t get_number_averages() const {
        return prm::n_cycles;
    }

    uint32_t get_fft_size() const {
        return prm::fft_size;
    }

    // Return the raw input value of each ADC channel
    // n_avg: number of averages
    std::array<int32_t, 2 * prm::n_adc> get_adc_raw_data(uint32_t n_avg) {
        if (n_avg <= 1) {
            return { sts.read<reg::adc00, int16_t>(),
                     sts.read<reg::adc01, int16_t>(),
                     sts.read<reg::adc10, int16_t>(),
                     sts.read<reg::adc11, int16_t>() };
        } else {
            int32_t adc00 = 0;
            int32_t adc01 = 0;
            int32_t adc10 = 0;
            int32_t adc11 = 0;

            for (size_t i=0; i<n_avg; i++) {
                adc00 += sts.read<reg::adc00, int16_t>();
                adc01 += sts.read<reg::adc01, int16_t>();
                adc10 += sts.read<reg::adc00, int16_t>();
                adc11 += sts.read<reg::adc01, int16_t>();
            }

            return { int32_t(std::round(adc00 / double(n_avg))),
                     int32_t(std::round(adc01 / double(n_avg))),
                     int32_t(std::round(adc10 / double(n_avg))),
                     int32_t(std::round(adc11 / double(n_avg))) };
        }
    }

    auto get_control_parameters() {
        return std::make_tuple(fs_adc, input_channel, W1, W2);
    }

    auto get_window_index() const {
        return window_index;
    }

 private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::psd0>& psd_map0;
    Memory<mem::demod0>& demod_map0;
    Memory<mem::psd1>& psd_map1;
    Memory<mem::demod1>& demod_map1;
    ClockGenerator& clk_gen;
    Ltc2157& ltc2157;

    double fs_adc; // ADC sampling rate (Hz)
    std::array<std::array<float, prm::fft_size/2>, 4> freq_calibration; // Conversion to W/Hz

    std::array<double, prm::fft_size> window;
    double W1, W2; // Window correction factors
    uint32_t window_index;

    uint32_t input_channel = 0;

    std::array<std::array<float, prm::fft_size/2>, 2> psd_buffer_raw;
    std::array<std::array<float, prm::fft_size/2>, 2> psd_buffer;
    std::array<std::thread, 2> psd_thread;
    std::mutex mutex;
    std::array<std::atomic<bool>, 2> psd_acquisition_started{};
    std::array<std::atomic<uint32_t>, 2> acq_cycle_index{};

    template <uint32_t adc> void psd_acquisition_thread();
    template <uint32_t adc>void start_psd_acquisition();

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
        fs_adc = clk_gen.get_adc_sampling_freq();
        const auto conv_factor = prm::n_cycles * fs_adc * load * W2;

        const auto Hinv = koheron::make_array(
            ltc2157.get_inverse_transfer_function<0, 0, prm::fft_size/2>(fs_adc),
            ltc2157.get_inverse_transfer_function<0, 1, prm::fft_size/2>(fs_adc),
            ltc2157.get_inverse_transfer_function<1, 0, prm::fft_size/2>(fs_adc),
            ltc2157.get_inverse_transfer_function<1, 1, prm::fft_size/2>(fs_adc)
        );

        const std::array<double, 4> vin = { ltc2157.get_input_voltage_range(0, 0),
                                            ltc2157.get_input_voltage_range(0, 1),
                                            ltc2157.get_input_voltage_range(1, 0),
                                            ltc2157.get_input_voltage_range(1, 1) };

        for (unsigned int i=0; i<prm::fft_size/2; ++i) {
            for (unsigned int j=0; j < freq_calibration.size(); ++j) {
                const auto vin_scal = vin[j] / (2 << 20);
                freq_calibration[j][i] = vin_scal * vin_scal * double(Hinv[j][i]) / conv_factor;
            }
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

        // For now the same window is used on both ADCs
        demod_map0.write_array(window_buffer);
        demod_map1.write_array(window_buffer);

        W1 = (res1 / prm::fft_size) * (res1 / prm::fft_size);
        W2 = res2 / prm::fft_size;
        set_conversion_vectors();
    }

    auto get_cycle_index(uint32_t adc) {
        return adc == 0 ? sts.read<reg::cycle_index0>()
                        : sts.read<reg::cycle_index1>();
    }

}; // class FFT

template <uint32_t adc>
inline void FFT::start_psd_acquisition() {
    if (! psd_acquisition_started[adc]) {
        psd_buffer[adc].fill(0.0);
        psd_thread[adc] = std::thread{&FFT::psd_acquisition_thread<adc>, this};
        psd_thread[adc].detach();
    }
}

template <uint32_t adc>
inline void  FFT::psd_acquisition_thread() {
    static_assert(adc < 2, "");
    using namespace std::chrono_literals;

    psd_acquisition_started[adc] = true;

    while (psd_acquisition_started[adc]) {
        uint32_t cycle_index = get_cycle_index(adc);
        uint32_t previous_cycle_index = cycle_index;

        // Wait for data
        while (cycle_index >= previous_cycle_index) {
            const auto sleep_time = std::chrono::nanoseconds((prm::n_cycles - cycle_index) * prm::fft_size * 4);
    
            if (sleep_time > 1ms) {
                std::this_thread::sleep_for(sleep_time);
            }

            previous_cycle_index = cycle_index;
            cycle_index = get_cycle_index(adc);
        }

        {
            std::lock_guard<std::mutex> lock(mutex);

            if (adc == 0) {
                psd_buffer_raw[adc] = psd_map0.read_array<float, prm::fft_size/2, 0>();
            } else { // adc == 1
                psd_buffer_raw[adc] = psd_map1.read_array<float, prm::fft_size/2, 0>();
            }

            if (std::abs(clk_gen.get_adc_sampling_freq() - fs_adc) > std::numeric_limits<double>::round_error()) {
                // Sampling frequency has changed
                set_conversion_vectors();
            }

            for (unsigned int i=0; i<prm::fft_size/2; i++) {
                psd_buffer[adc][i] = psd_buffer_raw[adc][i] * freq_calibration[(adc << 1) + input_channel][i];
            }
        }

        acq_cycle_index[adc] = get_cycle_index(adc);
    }
}

#endif // __DRIVERS_FFT_HPP__
