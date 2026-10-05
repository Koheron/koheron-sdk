#pragma once

#include "fft-board.hpp"
#include "server/drivers/fft/core.hpp"
#include <tuple>
#include "boards/alpha250/drivers/power-monitor.hpp"
#include "boards/alpha250/drivers/precision-adc.hpp"
#include "boards/alpha250/drivers/precision-dac.hpp"
#include "boards/alpha250/drivers/temperature-sensor.hpp"
#include "boards/alpha250/drivers/phase-modulator.hpp"

// Keep the public method order and response shapes stable for existing clients.
class FFT {
 public:
    FFT();
    void set_input_channel(uint32_t channel) { core.set_input_channel(channel); }
    void set_scale_sch(uint32_t scale_sch) { core.set_scale_sch(scale_sch); }
    void set_fft_window(uint32_t window_id) { core.set_fft_window(window_id); }
    auto read_psd_raw() { return core.read_psd_raw(); }
    auto read_psd() { return core.read_psd(); }
    uint32_t get_number_averages() const { return prm::n_cycles; }
    uint32_t get_fft_size() const { return prm::fft_size; }
    std::array<int32_t, prm::n_adc> get_adc_raw_data(uint32_t n_avg) {
        return core.get_adc_raw_data(n_avg);
    }
    void set_dds_freq(uint32_t channel, double freq_hz) {
        // Keep the legacy RPC while sharing the acknowledged DAC controller.
        rt::get_driver<PhaseModulator>().set_carrier_frequency(channel, freq_hz);
    }
    auto get_control_parameters() {
        const auto s = core.get_control_state();
        auto& generator = rt::get_driver<PhaseModulator>();
        const auto frequency = [&generator](uint32_t channel) {
            const auto settings = generator.get_settings_words(channel);
            return std::ldexp(double(std::get<1>(settings)), -int(generator.get_phase_width(channel))) * generator.get_sample_rate();
        };
        return std::tuple{frequency(0), frequency(1), s.sample_rate, s.channel, s.w1, s.w2, s.window, rt::get_driver<ClockGenerator>().get_reference_clock()};
    }
    auto get_board_parameters() {
        auto supplies = rt::get_driver<PowerMonitor>().get_supplies_ui(); // std::array<float, 4>
        auto adc_values = rt::get_driver<PrecisionAdc>().get_adc_values(); // std::array<float, 8>
        auto dac_values = rt::get_driver<PrecisionDac>().get_dac_values(); // std::array<float, 4>
        auto temperatures = rt::get_driver<TemperatureSensor>().get_temperatures(); // std::array<float, 3>

        // Concatenate the arrays
        using namespace scicpp::operators;
        return supplies | adc_values | dac_values | temperatures;
    }

 private:
    fft::Core<Alpha250FftBoard> core;
};
