#pragma once

#include "server/hardware/memory_manager.hpp"
#include "server/runtime/syslog.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <thread>
#include <scicpp/signal.hpp>

namespace fft {

// Board supplies the ADC calibration, sampling clock and window defaults.
// Network-facing wrappers retain each instrument's existing wire protocol.
template <typename Board>
class Core {
 public:
    Core() {
        set_input_channel(0);
        set_scale_sch(0);
        set_fft_window(1);
        ctl.template set_bit<reg::psd_valid, 0>();
        running = true;
        worker = std::thread(&Core::acquire, this);
    }

    ~Core() {
        running = false;
        if (worker.joinable()) { worker.join(); }
    }

    void set_input_channel(uint32_t channel) {
        std::lock_guard lock(mutex);
        if (channel >= 2) {
            log<ERROR>("FFT::set_input_channel invalid channel\n");
            return;
        }
        input_channel = channel;
        ctl.template write<reg::psd_input_sel>(channel);
    }

    void set_scale_sch(uint32_t scale_sch) {
        ctl.template write<reg::ctl_fft>(1 + (scale_sch << 1));
    }

    void set_fft_window(uint32_t id) {
        namespace win = scicpp::signal::windows;
        std::lock_guard lock(mutex);
        std::array<double, prm::fft_size> window;
        switch (id) {
        case 0: window = win::boxcar<double, prm::fft_size>(); break;
        case 1: window = win::hann<double, prm::fft_size>(); break;
        case 2: window = win::flattop<double, prm::fft_size>(); break;
        case 3: window = win::blackmanharris<double, prm::fft_size>(); break;
        default:
            log<ERROR>("FFT: Invalid window index\n");
            return;
        }
        hw::get_memory<mem::demod>().write_array(scicpp::map([](auto w) {
            return uint32_t(((int32_t(32768 * w) + 32768) % 65536) + 32768);
        }, window));
        W1 = win::s1(window) / prm::fft_size / prm::fft_size;
        W2 = win::s2(window) / prm::fft_size;
        update_conversion();
        window_index = id;
    }

    auto read_psd_raw() {
        std::lock_guard lock(mutex);
        return raw;
    }

    auto read_psd() {
        std::lock_guard lock(mutex);
        return psd;
    }

    std::array<int32_t, prm::n_adc> get_adc_raw_data(uint32_t n_avg) {
        std::array<int64_t, 2> sum{};
        const auto count = std::max(n_avg, uint32_t{1});
        for (uint32_t i = 0; i < count; ++i) {
            sum[0] += signed_adc(sts.template read<reg::adc0>());
            sum[1] += signed_adc(sts.template read<reg::adc1>());
        }
        return {int32_t(std::round(sum[0] / double(count))),
                int32_t(std::round(sum[1] / double(count)))};
    }

    void set_dds_freq(uint32_t channel, double frequency) {
        std::lock_guard lock(mutex);
        set_dds_unlocked(channel, frequency);
    }

    struct ControlState {
        std::array<double, 2> dds;
        double sample_rate;
        uint32_t channel;
        double w1, w2;
        uint32_t window;
    };

    ControlState get_control_state() {
        std::lock_guard lock(mutex);
        return {dds, fs, input_channel, W1, W2, window_index};
    }

 private:
    static int32_t signed_adc(uint32_t value) {
        constexpr uint32_t sign = 1U << (prm::adc_width - 1);
        return int32_t(value & (sign - 1)) - int32_t(value & sign);
    }

    void set_dds_unlocked(uint32_t channel, double frequency) {
        if (channel >= 2 || std::isnan(frequency)) {
            log<ERROR>("FFT::set_dds_freq invalid channel or frequency\n");
            return;
        }
        frequency = std::clamp(frequency, 0.0, fs / 2);
        ctl.write_reg(reg::phase_incr0 + 4 * channel,
                      uint32_t((uint64_t{1} << 32) / fs * frequency));
        dds[channel] = frequency;
    }

    // Called with mutex held, including initialization before worker startup.
    void update_conversion() {
        fs = board.sampling_frequency();
        auto& adc = board.calibration();
        const auto inverse = std::array{
            adc.template get_inverse_transfer_function<0, prm::fft_size/2>(fs),
            adc.template get_inverse_transfer_function<1, prm::fft_size/2>(fs)};
        for (uint32_t channel = 0; channel < 2; ++channel) {
            const double voltage = adc.get_input_voltage_range(channel);
            // Both existing protocols express PSD as equivalent power in 50 ohms.
            const float factor = (voltage / (2 << 20)) * (voltage / (2 << 20))
                               / prm::n_cycles / fs / 50.0 / W2;
            for (size_t i = 0; i < psd.size(); ++i) {
                conversion[channel][i] = factor * inverse[channel][i];
            }
        }
    }

    void acquire() {
        using namespace std::chrono_literals;
        while (running) {
            auto cycle = sts.template read<reg::cycle_index>();
            auto previous = cycle;
            while (running && cycle >= previous) {
                double rate;
                {
                    std::lock_guard lock(mutex);
                    rate = fs;
                }
                const auto remaining = prm::n_cycles - std::min(cycle, uint32_t(prm::n_cycles));
                const auto delay = std::chrono::duration<double>(remaining * double(prm::fft_size) / rate);
                if (delay > 1ms) {
                    std::this_thread::sleep_for(std::min(delay, std::chrono::duration<double>(.05)));
                } else {
                    std::this_thread::yield();
                }
                previous = cycle;
                cycle = sts.template read<reg::cycle_index>();
            }
            if (!running) { break; }
            std::lock_guard lock(mutex);
            raw = hw::get_memory<mem::psd>().template read_array<float, prm::fft_size/2>();
            if (std::abs(board.sampling_frequency() - fs) > .5) {
                update_conversion();
                set_dds_unlocked(0, dds[0]);
                set_dds_unlocked(1, dds[1]);
            }
            for (size_t i = 0; i < psd.size(); ++i) {
                psd[i] = raw[i] * conversion[input_channel][i];
            }
        }
    }

    Board board;
    hw::Memory<mem::control>& ctl = hw::get_memory<mem::control>();
    hw::Memory<mem::status>& sts = hw::get_memory<mem::status>();
    double fs = prm::adc_clk;
    double W1 = 0, W2 = 0;
    uint32_t window_index = 0, input_channel = 0;
    std::array<double, 2> dds{};
    std::array<std::array<float, prm::fft_size/2>, 2> conversion{};
    std::array<float, prm::fft_size/2> raw{}, psd{};
    std::mutex mutex;
    std::atomic<bool> running{false};
    std::thread worker;
};
} // namespace fft
