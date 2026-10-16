#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class BistFull
{
  public:
    BistFull();
    ~BistFull();
    bool start(uint32_t analog_mask, uint32_t budget_ms, uint32_t seed);
    void cancel();
    std::string get_report();
    uint32_t ping(uint32_t value);
    uint64_t echo_u64(uint64_t value);
    double echo_double(double value);
    std::vector<uint32_t> echo_vector(std::vector<uint32_t> values);
    bool recover_devices();
    bool record_host_results(std::string run_id, std::string results_json);

  private:
    friend class FullCompletion;
    bool wait(uint32_t timeout_ms);
    struct Impl;
    std::unique_ptr<Impl> impl;
};
