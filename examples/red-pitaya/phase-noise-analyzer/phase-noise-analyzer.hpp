#pragma once
#include "phase-noise-board.hpp"
#include "server/drivers/phase-noise/core.hpp"

// Keep the analyzer RPC method order and response shapes common across boards.
class PhaseNoiseAnalyzer {
 public:
    PhaseNoiseAnalyzer() = default;
    void save_config() { core.save_config(); }
    void set_local_oscillator(uint32_t channel, double freq_hz) { core.set_local_oscillator(channel, freq_hz); }
    void set_cic_rate(uint32_t rate) { core.set_cic_rate(rate); }
    void set_channel(uint32_t chan) { core.set_channel(chan); }
    void set_fft_navg(uint32_t n_avg) { core.set_fft_navg(n_avg); }
    void set_analyzer_mode(uint32_t mode) { core.set_analyzer_mode(mode); }
    void set_interferometer_delay(float delay_s) { core.set_interferometer_delay(delay_s); }
    void set_tracking_enabled(bool enabled) { core.set_tracking_enabled(enabled); }
    void set_tracking_bandwidth(float bandwidth_hz) { core.set_tracking_bandwidth(bandwidth_hz); }
    void set_tracking_max_step(float max_step_hz) { core.set_tracking_max_step(max_step_hz); }
    void set_tracking_max_correction(float max_correction_hz) { core.set_tracking_max_correction(max_correction_hz); }
    auto get_tracking_parameters() { return core.get_tracking_parameters(); }
    auto get_parameters() { return core.get_parameters(); }
    double get_carrier_power(uint32_t navg) { return core.get_carrier_power(navg); }
    auto get_jitter() { return core.get_jitter(); }
    auto get_measurements(uint32_t navg) { return core.get_measurements(navg); }
    auto get_phase() const { return core.get_phase(); }
    auto get_phase_noise() const { return core.get_phase_noise(); }
    bool set_phase_precision(uint32_t bits) { return core.set_phase_precision(bits); }
    auto get_precision_status() { return core.get_precision_status(); }
    auto get_phase_snapshot() const { return core.get_phase_snapshot(); }
    auto get_average_status() const { return core.get_average_status(); }

    auto get_spectrum_snapshot() const { return core.get_spectrum_snapshot(); }
    auto get_dma_status() { return core.get_dma_status(); }
    auto get_stream_status() { return core.get_stream_status(); }

 private:
    phase_noise::Core<RedPitayaPhaseNoiseBoard> core;
};
