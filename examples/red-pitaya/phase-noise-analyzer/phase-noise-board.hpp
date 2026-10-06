#pragma once
#include "dds.hpp"
#include "server/hardware/memory_manager.hpp"

struct RedPitayaPhaseNoiseBoard {
    void initialize_phase() { hw::get_memory<mem::control>().write_mask<reg::cordic, 3>(3); }
    void select_channel(uint32_t channel) {
        hw::get_memory<mem::control>().write_mask<reg::cordic, 16>((channel & 1) << 4);
    }
    uint32_t demodulated(uint32_t channel) {
        auto& status = hw::get_memory<mem::status>();
        return channel == 0 ? status.read<reg::demod0>() : status.read<reg::demod1>();
    }
    using Oscillator = Dds;
    static constexpr uint32_t max_phase_precision = 8;
    static constexpr uint32_t cic_rate_step = 2;
    static_assert(prm::phase_filter_width == 40);
    void set_phase_precision(uint32_t bits) {
        hw::get_memory<mem::control>().write<reg::phase_precision>(bits);
    }
    uint32_t phase_packet_status() {
        return hw::get_memory<mem::status>().read<reg::phase_packet>();
    }
    double sampling_frequency() { return prm::adc_clk; }
    uint32_t reference_clock() { return 0; } // fixed on-board oscillator
    double power_conversion(uint32_t, double) {
        // Nominal +/-1 V input (LV jumper); 14-bit ADC shifted left by two.
        // The 33 -> 24-bit complex product shifts 9 bits; the status path
        // drops another 8 after filtering. Mixing halves carrier amplitude,
        // so legacy status IQ / 65536 still equals Vpeak / 16.
        // Convert Vpeak^2 / (2 * 50 ohm) to mW. Board gain is uncalibrated.
        return 256.0 / (2.0 * 50.0 * .001);
    }
};
