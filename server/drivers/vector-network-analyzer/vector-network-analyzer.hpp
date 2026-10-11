#ifndef KOHERON_VECTOR_NETWORK_ANALYZER_HPP
#define KOHERON_VECTOR_NETWORK_ANALYZER_HPP
#include "capture.hpp"
#include "vna-clock.hpp"
// Board adapters share acquisition ownership, sweep planning and DMA checks.
class VectorNetworkAnalyzer {
  public:
    std::tuple<uint32_t, uint32_t> begin_sweep(double start, double stop, double duration, double level) {
        return capture.begin_sweep(start, stop, duration, level);
    }
    std::array<uint32_t, 36> get_plan(uint32_t token) { return capture.get_plan(token); }
    uint32_t get_status(uint32_t token) { return capture.get_status(token); }
    std::vector<uint32_t> get_adc_block(uint32_t token, uint32_t index) { return capture.get_adc_block(token, index); }
    bool release(uint32_t token) { return capture.release(token); }
    // Append RPCs to retain the original full-record client protocol.
    std::tuple<uint32_t, uint32_t> begin_recorded_sweep(double start, double stop, double duration, double level, uint32_t samples) {
        return capture.begin_sweep(start, stop, duration, level, samples);
    }
    uint32_t get_record_samples(uint32_t token) { return capture.get_record_samples(token); }
    std::vector<uint32_t> get_adc_blocks(uint32_t token, uint32_t index, uint32_t count) {
        return capture.get_adc_blocks(token, index, count);
    }
  private:
    VNACapture<VNAClock> capture;
};
#endif
