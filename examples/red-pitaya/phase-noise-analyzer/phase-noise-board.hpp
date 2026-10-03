#pragma once
#include "dds.hpp"
#include "server/hardware/memory_manager.hpp"

struct RedPitayaPhaseNoiseBoard {
    using Oscillator = Dds;
    double sampling_frequency() { return prm::adc_clk; }
    uint32_t reference_clock() { return 0; } // fixed on-board oscillator
    double power_conversion(uint32_t, double) {
        // Nominal +/-1 V input (LV jumper); 14-bit ADC shifted left by two.
        // The 33 -> 16-bit complex product shifts 17 bits, then mixing halves
        // the carrier amplitude. IQ / 65536 therefore equals Vpeak / 16.
        // Convert Vpeak^2 / (2 * 50 ohm) to mW. Board gain is uncalibrated.
        return 256.0 / (2.0 * 50.0 * .001);
    }
};
