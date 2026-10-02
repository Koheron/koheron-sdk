#ifndef ALPHA250_PHASE_MODULATOR_HPP
#define ALPHA250_PHASE_MODULATOR_HPP

#include "server/hardware/memory_manager.hpp"
#include "server/drivers/dds/phase-modulator.hpp"
#include "boards/alpha250/drivers/clock-generator.hpp"
#include <cstdint>
#include <string>
#include <tuple>

class PhaseModulator {
public:
    PhaseModulator() : controller(hw::get_memory<mem::awg>(), static_cast<long double>(prm::adc_clk)) {
        if (!controller.valid()) logf<ERROR>("PhaseModulator: {}\n", controller.initialization_result().message());
        rt::get_driver<ClockGenerator>().set_sampling_frequency(1); // 250 MS/s
    }

    uint32_t get_channel_count() { return controller.channel_count(); }
    uint32_t get_sample_rate() { return prm::adc_clk; }
    uint32_t get_phase_width(uint32_t channel) { return controller.phase_width(channel); }
    uint32_t get_capabilities(uint32_t channel) { return controller.capabilities(channel); }

    std::string get_initialization_error() { return controller.initialization_result().message(); }
    auto get_channel_info(uint32_t channel) {
        const auto info = controller.channel_info(channel);
        return std::tuple{info.phase_width, info.modulation_width, info.lut_bits,
                          info.prbs_width, info.output_width, info.capabilities};
    }

    auto get_settings_words(uint32_t channel) {
        dds_pm::Settings settings;
        const auto result = controller.get_settings(channel, settings);
        return std::tuple{static_cast<uint32_t>(result.code), settings.carrier_increment,
            settings.carrier_phase, settings.modulation_increment, settings.modulation_phase,
            settings.deviation, settings.duty, settings.seed, static_cast<uint32_t>(settings.waveform),
            settings.output_enabled, settings.pm_enabled};
    }
    std::string get_error_message(uint32_t code) {
        return dds_pm::Result{static_cast<dds_pm::Error>(code)}.message();
    }

    std::string configure_words_checked(uint32_t channel, uint64_t carrier_increment, uint64_t carrier_phase,
                         uint64_t modulation_increment, uint64_t modulation_phase,
                         uint64_t deviation, uint64_t duty, uint32_t seed, uint32_t waveform,
                         bool output_enabled, bool pm_enabled, bool restart_carrier,
                         bool restart_modulation) {
        dds_pm::Settings settings;
        settings.carrier_increment = carrier_increment;
        settings.carrier_phase = carrier_phase;
        settings.modulation_increment = modulation_increment;
        settings.modulation_phase = modulation_phase;
        settings.deviation = deviation;
        settings.duty = duty;
        settings.seed = seed;
        settings.waveform = static_cast<dds_pm::Waveform>(waveform);
        settings.output_enabled = output_enabled;
        settings.pm_enabled = pm_enabled;
        const auto result = controller.configure(channel, settings, restart_carrier, restart_modulation);
        return response(channel, result);
    }
    // Boolean RPC retained for existing native-word clients.
    bool configure_words(uint32_t channel, uint64_t carrier_increment, uint64_t carrier_phase,
                         uint64_t modulation_increment, uint64_t modulation_phase,
                         uint64_t deviation, uint64_t duty, uint32_t seed, uint32_t waveform,
                         bool output_enabled, bool pm_enabled, bool restart_carrier,
                         bool restart_modulation) {
        return configure_words_checked(channel, carrier_increment, carrier_phase,
            modulation_increment, modulation_phase, deviation, duty, seed, waveform,
            output_enabled, pm_enabled, restart_carrier, restart_modulation).empty();
    }

    std::string configure(uint32_t channel, double carrier_hz, double carrier_phase_deg,
                          double modulation_hz, double modulation_phase_deg, double deviation_deg,
                          double duty, uint32_t seed, uint32_t waveform, bool output_enabled,
                          bool pm_enabled, bool restart) {
        dds_pm::SignalSettings signal;
        signal.carrier_hz = static_cast<long double>(carrier_hz);
        signal.carrier_phase_deg = static_cast<long double>(carrier_phase_deg);
        signal.modulation_hz = static_cast<long double>(modulation_hz);
        signal.modulation_phase_deg = static_cast<long double>(modulation_phase_deg);
        signal.deviation_deg = static_cast<long double>(deviation_deg);
        signal.duty = static_cast<long double>(duty);
        signal.seed = seed;
        signal.waveform = static_cast<dds_pm::Waveform>(waveform);
        signal.output_enabled = output_enabled;
        signal.pm_enabled = pm_enabled;
        return response(channel, controller.configure_signal(channel, signal, prm::adc_clk, restart, restart));
    }
    std::string set_carrier_increment(uint32_t channel, uint64_t word) {
        return response(channel, controller.set_carrier_increment(channel, word));
    }
    std::string set_modulation_increment(uint32_t channel, uint64_t word) {
        return response(channel, controller.set_modulation_increment(channel, word));
    }
    std::string set_phase_word(uint32_t channel, uint64_t word) {
        return response(channel, controller.set_phase_word(channel, word));
    }
    std::string set_deviation_word(uint32_t channel, uint64_t word) {
        return response(channel, controller.set_deviation_word(channel, word));
    }
    std::string mute(uint32_t channel) { return response(channel, controller.mute(channel)); }
    std::string set_output_enabled(uint32_t channel, bool enabled) {
        return response(channel, controller.set_output_enabled(channel, enabled));
    }
    std::string restart(uint32_t channel) { return response(channel, controller.restart(channel)); }
    std::string set_pm_enabled(uint32_t channel, bool enabled) {
        return response(channel, controller.set_pm_enabled(channel, enabled));
    }
    std::string set_carrier_frequency(uint32_t channel, double hz) {
        return response(channel, controller.set_carrier_frequency(channel, static_cast<long double>(hz), prm::adc_clk));
    }
    std::string set_modulation_frequency(uint32_t channel, double hz) {
        return response(channel, controller.set_modulation_frequency(channel, static_cast<long double>(hz), prm::adc_clk));
    }
    std::string set_phase(uint32_t channel, double degrees) {
        return response(channel, controller.set_phase(channel, static_cast<long double>(degrees)));
    }
    std::string set_deviation(uint32_t channel, double degrees) {
        return response(channel, controller.set_deviation(channel, static_cast<long double>(degrees)));
    }
    std::string set_waveform(uint32_t channel, uint32_t waveform) {
        return response(channel, controller.set_waveform(channel, static_cast<dds_pm::Waveform>(waveform)));
    }
    std::string set_duty(uint32_t channel, double fraction) {
        return response(channel, controller.set_duty(channel, static_cast<long double>(fraction)));
    }
    std::string set_seed(uint32_t channel, uint32_t seed) {
        return response(channel, controller.set_seed(channel, seed));
    }

private:
    std::string response(uint32_t channel, dds_pm::Result result) {
        if (!result) logf<ERROR>("PhaseModulator channel {}: {}\n", channel, result.message());
        return result.message();
    }
    dds_pm::Controller<hw::Memory<mem::awg>> controller;
};
#endif
