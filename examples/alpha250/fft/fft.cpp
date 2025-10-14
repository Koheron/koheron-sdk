#include "./fft.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/driver_manager.hpp"
#include "boards/alpha250/drivers/clock-generator.hpp"
#include "boards/alpha250/drivers/ltc2157.hpp"

#include <cmath>
#include <limits>
#include <chrono>
#include <scicpp/core.hpp>
#include <scicpp/signal.hpp>

namespace sci = scicpp;
namespace win = scicpp::signal::windows;

using services::require;

FFT::FFT()
: ctl(require<hw::MemoryManager>().get<mem::control>())
, sts(require<hw::MemoryManager>().get<mem::status>())
, clk_gen(require<rt::DriverManager>().get<ClockGenerator>())
{
    set_input_channel(0);
    set_scale_sch(0);
    set_fft_window(1);
    ctl.set_bit<reg::psd_valid, 0>();
    start_psd_acquisition();
}

void FFT::set_input_channel(uint32_t channel) {
    if (channel >= 2) {
        log<ERROR>("FFT::set_input_channel invalid channel\n");
        return;
    }

    input_channel = channel;
    ctl.write<reg::psd_input_sel>(channel);
}

void FFT::set_scale_sch(uint32_t scale_sch) {
    // LSB at 1 for forward FFT
    ctl.write<reg::ctl_fft>(1 + (scale_sch << 1));
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

std::array<int32_t, prm::n_adc> FFT::get_adc_raw_data(uint32_t n_avg) {
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

void FFT::set_dds_freq(uint32_t channel, double freq_hz) {
    if (channel >= 2) {
        log<ERROR>("FFT::set_dds_freq invalid channel\n");
        return;
    }

    if (std::isnan(freq_hz)) {
        log<ERROR>("FFT::set_dds_freq Frequency is NaN\n");
        return;
    }

    if (freq_hz > fs_adc / 2) {
        freq_hz = fs_adc / 2;
    }

    if (freq_hz < 0.0) {
        freq_hz = 0.0;
    }

    double factor = (uint64_t(1) << 32) / fs_adc;
    ctl.write_reg(reg::phase_incr0 + 4 * channel, uint32_t(factor * freq_hz));
    dds_freq[channel] = freq_hz;
}

void FFT::set_conversion_vectors() {
    constexpr double load = 50.0; // Ohm

    fs_adc = clk_gen.get_adc_sampling_freq();
    auto& ltc2157 = require<rt::DriverManager>().get<Ltc2157>();

    auto Hinv = std::array{
        ltc2157.get_inverse_transfer_function<0, prm::fft_size/2>(fs_adc),
        ltc2157.get_inverse_transfer_function<1, prm::fft_size/2>(fs_adc)
    };

    std::array<double, 2> vin = { ltc2157.get_input_voltage_range(0),
                                  ltc2157.get_input_voltage_range(1) };

    float C0 =  (vin[0] / (2 << 20)) * (vin[0] / (2 << 20)) / prm::n_cycles / fs_adc / load / W2;
    float C1 =  (vin[1] / (2 << 20)) * (vin[1] / (2 << 20)) / prm::n_cycles / fs_adc / load / W2;

    for (unsigned int i=0; i<prm::fft_size/2; i++) {
        freq_calibration[0][i] = C0 * Hinv[0][i];
        freq_calibration[1][i] = C1 * Hinv[1][i];
    }
}

void FFT::set_window(const std::array<double, prm::fft_size> &window) {
    auto& demod_map = require<hw::MemoryManager>().get<mem::demod>();
    demod_map.write_array(sci::map([](auto w){
        return uint32_t(((int32_t(32768 * w) + 32768) % 65536) + 32768);
    }, window));

    S1 = win::s1(window);
    S2 = win::s2(window);
    ENBW = win::enbw(window);

    W1 = S1 / prm::fft_size / prm::fft_size;
    W2 = S2 / prm::fft_size;
    set_conversion_vectors();
}

uint32_t FFT::get_cycle_index() {
    return sts.read<reg::cycle_index>();
}

void FFT::start_psd_acquisition() {
    if (! psd_acquisition_started) {
        psd_buffer.fill(0);
        psd_thread = std::thread{&FFT::psd_acquisition_thread, this};
        psd_thread.detach();
    }
}

void  FFT::psd_acquisition_thread() {
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
            auto& psd_map = require<hw::MemoryManager>().get<mem::psd>();
            psd_buffer_raw = psd_map.read_array<float, prm::fft_size/2>();

            if (std::abs(clk_gen.get_adc_sampling_freq() - fs_adc) > std::numeric_limits<double>::round_error()) {
                // Sampling frequency has changed
                set_conversion_vectors();
                set_dds_freq(0, dds_freq[0]);
                set_dds_freq(1, dds_freq[1]);
            }

            for (unsigned int i=0; i<prm::fft_size/2; i++) {
                psd_buffer[i] = psd_buffer_raw[i] * freq_calibration[input_channel][i];
            }
        }

        acq_cycle_index = get_cycle_index();
    }
}