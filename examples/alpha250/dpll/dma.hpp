#pragma once
#include "monitor-board.hpp"
#include "server/drivers/phase-noise/core.hpp"

class Dma {
  public:
    // Preserve the four original RPC IDs and raw capture's fixed array shape.
    void set_cic_rate(uint32_t rate) { core.set_cic_rate(rate); }
    const auto& get_data() {
        raw_valid = core.copy_raw_capture(raw);
        if (!raw_valid) {
            raw.fill(0);
            log<ERROR>("DPLL raw phase capture interrupted; use get_raw_capture_valid before accepting it\n");
        }
        return raw;
    }
    uint32_t get_data_size() { return raw.size(); }
    uint32_t get_sampling_frequency() { return std::get<1>(core.get_parameters()).eval(); }

    void set_channel(uint32_t channel) { core.set_channel(channel); }
    void set_fft_navg(uint32_t count) { core.set_fft_navg(count); }
    bool set_phase_precision(uint32_t bits) { return core.set_phase_precision(bits); }
    auto get_parameters() { return core.get_parameters(); }
    auto get_spectrum_snapshot() const { return core.get_spectrum_snapshot(); }
    auto get_phase_noise() const { return core.get_phase_noise(); }
    auto get_average_status() const { return core.get_average_status(); }
    auto get_precision_status() { return core.get_precision_status(); }
    auto get_stream_status() { return core.get_stream_status(); }
    auto get_phase_snapshot() const { return core.get_phase_snapshot(); }
    auto get_dma_status() { return core.get_dma_status(); }
    auto get_measurements(uint32_t navg) { return core.get_measurements(navg); }
    void reset_average() { core.reset_average(); }
    bool get_raw_capture_valid() const { return raw_valid; }
    auto get_stream_coverage() const { return core.get_stream_coverage(); }
    auto get_stream_performance() { return core.get_stream_performance(); }
    auto get_fft_performance() const { return core.get_fft_performance(); }

  private:
    phase_noise::Core<DpllMonitorBoard> core;
    std::array<int32_t, 1000000> raw{};
    bool raw_valid = false;
};
