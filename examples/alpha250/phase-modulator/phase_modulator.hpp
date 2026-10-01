#ifndef ALPHA250_PHASE_MODULATOR_HPP
#define ALPHA250_PHASE_MODULATOR_HPP

#include "server/hardware/memory_manager.hpp"
#include "server/drivers/dds/phase-modulator.hpp"
#include "boards/alpha250/drivers/clock-generator.hpp"
#include <cstdint>

class PhaseModulator {
public:
    PhaseModulator()
        : channel0(hw::get_memory<mem::awg0>()), channel1(hw::get_memory<mem::awg1>()) {
        rt::get_driver<ClockGenerator>().set_sampling_frequency(1); // 250 MS/s
    }

    uint32_t get_sample_rate() { return prm::adc_clk; }
    uint32_t get_phase_width(uint32_t channel) {
        if (channel > 1) return 0;
        return channel == 0 ? channel0.phase_width() : channel1.phase_width();
    }
    uint32_t get_capabilities(uint32_t channel) {
        if (channel > 1) return 0;
        return channel == 0 ? channel0.capabilities() : channel1.capabilities();
    }

    bool configure_words(uint32_t channel, uint64_t carrier_increment, uint64_t carrier_phase,
                         uint64_t modulation_increment, uint64_t modulation_phase,
                         uint64_t deviation, uint64_t duty, uint32_t seed, uint32_t waveform,
                         bool output_enabled, bool pm_enabled, bool restart_carrier,
                         bool restart_modulation) {
        if (channel > 1) return false;
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
        const bool result = channel == 0 ? channel0.configure(settings, restart_carrier, restart_modulation) :
                                          channel1.configure(settings, restart_carrier, restart_modulation);
        if (!result) logf<ERROR>("PhaseModulator: {}\n", channel == 0 ? channel0.error() : channel1.error());
        return result;
    }
private:
    dds_pm::Controller<hw::Memory<mem::awg0>> channel0;
    dds_pm::Controller<hw::Memory<mem::awg1>> channel1;
};
#endif
