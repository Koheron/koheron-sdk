#include "./fft.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/driver_manager.hpp"
#include "boards/alpha250-4/drivers/clock-generator.hpp"
#include "boards/alpha250-4/drivers/ltc2157.hpp"

#include <chrono>
#include <cmath>
#include <limits>
#include <scicpp/signal.hpp>

namespace win = scicpp::signal::windows;

FFT::FFT()
: ctl(hw::get_memory<mem::control>())
, sts(hw::get_memory<mem::status>())
, clk_gen(rt::get_driver<ClockGenerator>())
, ltc2157(rt::get_driver<Ltc2157>())
{
    set_input_channel(0);
    // Scaling schedule is ignored when FFT core configure for floating points
    set_scale_sch(0);
    set_fft_window(1);
    ctl.set_bit<reg::psd_valid0, 0>();
    ctl.set_bit<reg::psd_valid1, 0>();
    start_psd_acquisition<0>();
    start_psd_acquisition<1>();
}

void FFT::set_input_channel(uint32_t channel) {
    if (channel >= 2) {
        log<ERROR>("FFT::set_input_channel invalid channel\n");
        return;
    }

    input_channel = channel;
    ctl.write<reg::psd_input_sel0>(channel);
    ctl.write<reg::psd_input_sel1>(channel);
}

void FFT::set_scale_sch(uint32_t scale_sch) {
    // LSB at 1 for forward FFT
    ctl.write<reg::ctl_fft0>(1 + (scale_sch << 1));
    ctl.write<reg::ctl_fft1>(1 + (scale_sch << 1));
}

void FFT::set_fft_window(uint32_t window_id) {
    switch (window_id) {
    case 0:
        set_window(win::boxcar<double, prm::fft_size>());
        break;
    case 1:
        set_window(win::hann<double, prm::fft_size>());
        break;
    case 2:
        set_window(win::flattop<double, prm::fft_size>());
        break;
    case 3:
        set_window(win::blackmanharris<double, prm::fft_size>());
        break;
    default:
        log<ERROR>("FFT: Invalid window index\n");
        return;
    }

    window_index = window_id;
}

// Read averaged spectrum data
std::array<float, prm::fft_size/2> FFT::read_psd_raw(uint32_t adc) {
    if (adc >= 2) {
        log<ERROR>("FFT::read_psd_raw: Invalid adc\n");
        return std::array<float, prm::fft_size/2>{};
    }

    std::lock_guard<std::mutex> lock(mutex[adc]);
    return psd_buffer_raw[adc];
}

// Return the PSD in W/Hz
std::array<float, prm::fft_size/2> FFT::read_psd(uint32_t adc) {
    if (adc >= 2) {
        log<ERROR>("FFT::read_psd: Invalid adc\n");
        return std::array<float, prm::fft_size/2>{};
    }

    std::lock_guard<std::mutex> lock(mutex[adc]);
    return psd_buffer[adc];
}

std::array<int32_t, 2 * prm::n_adc> FFT::get_adc_raw_data(uint32_t n_avg) {
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
            adc10 += sts.read<reg::adc10, int16_t>();
            adc11 += sts.read<reg::adc11, int16_t>();
        }

        return { int32_t(std::round(adc00 / double(n_avg))),
                    int32_t(std::round(adc01 / double(n_avg))),
                    int32_t(std::round(adc10 / double(n_avg))),
                    int32_t(std::round(adc11 / double(n_avg))) };
    }
}

void FFT::set_conversion_vectors() {
    constexpr double load = 50.0; // Ohm
    fs_adc = clk_gen.get_adc_sampling_freq();

    const std::array<double, 4> conv_factor = {
        prm::n_cycles * fs_adc[0] * load * W2,
        prm::n_cycles * fs_adc[0] * load * W2,
        prm::n_cycles * fs_adc[1] * load * W2,
        prm::n_cycles * fs_adc[1] * load * W2
    };

    const auto Hinv = std::array{
        ltc2157.get_inverse_transfer_function<0, 0, prm::fft_size/2>(fs_adc[0]),
        ltc2157.get_inverse_transfer_function<0, 1, prm::fft_size/2>(fs_adc[0]),
        ltc2157.get_inverse_transfer_function<1, 0, prm::fft_size/2>(fs_adc[1]),
        ltc2157.get_inverse_transfer_function<1, 1, prm::fft_size/2>(fs_adc[1])
    };

    const std::array<double, 4> vin = {
        ltc2157.get_input_voltage_range(0, 0),
        ltc2157.get_input_voltage_range(0, 1),
        ltc2157.get_input_voltage_range(1, 0),
        ltc2157.get_input_voltage_range(1, 1)
    };

    for (unsigned int j=0; j < freq_calibration.size(); ++j) {
        const auto vin_scal = vin[j] / (2 << 20);
        const auto factor = 2.0 * vin_scal * vin_scal / conv_factor[j];

        for (unsigned int i=0; i < prm::fft_size/2; ++i) {
            freq_calibration[j][i] = factor * double(Hinv[j][i]);
        }
    }
}

void FFT::set_window(const std::array<double, prm::fft_size> &window) {
    std::array<uint32_t, prm::fft_size> window_buffer;

    for (size_t i=0; i<prm::fft_size; i++) {
        window_buffer[i] = ((int32_t(32768 * window[i]) + 32768) % 65536) + 32768;
    }

    // For now the same window is used on both ADCs
    hw::get_memory<mem::demod0>().write_array(window_buffer);
    hw::get_memory<mem::demod1>().write_array(window_buffer);

    S1 = win::s1(window);
    S2 = win::s2(window);
    ENBW = win::enbw(window);
    W1 = S1 / prm::fft_size / prm::fft_size;
    W2 = S2 / prm::fft_size;
    set_conversion_vectors();
}

uint32_t FFT::get_cycle_index(uint32_t adc) {
    return adc == 0 ? sts.read<reg::cycle_index0>()
                    : sts.read<reg::cycle_index1>();
}

template <uint32_t adc>
void FFT::start_psd_acquisition() {
    if (! psd_acquisition_started[adc]) {
        psd_buffer[adc].fill(0.0);
        psd_thread[adc] = std::thread{&FFT::psd_acquisition_thread<adc>, this};
        psd_thread[adc].detach();
    }
}

template <uint32_t adc>
void FFT::psd_acquisition_thread() {
    static_assert(adc < 2);
    using namespace std::chrono_literals;

    psd_acquisition_started[adc] = true;

    while (psd_acquisition_started[adc]) {
        uint32_t cycle_index = get_cycle_index(adc);
        uint32_t previous_cycle_index = cycle_index;

        // Wait for data
        while (cycle_index >= previous_cycle_index) {
            const auto sleep_time
                = std::chrono::nanoseconds((prm::n_cycles - cycle_index) * prm::fft_size * 4);
    
            if (sleep_time > 1ms) {
                std::this_thread::sleep_for(sleep_time);
            }

            previous_cycle_index = cycle_index;
            cycle_index = get_cycle_index(adc);
        }

        {
            std::lock_guard lock(mutex[adc]);

            if (adc == 0) {
                psd_buffer_raw[adc] = hw::get_memory<mem::psd0>().read_array<float, prm::fft_size/2, 0>();
            } else { // adc == 1
                psd_buffer_raw[adc] = hw::get_memory<mem::psd1>().read_array<float, prm::fft_size/2, 0>();
            }

            if (std::abs(clk_gen.get_adc_sampling_freq()[adc] - fs_adc[adc])
                    > std::numeric_limits<double>::round_error()) {
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