#ifndef KOHERON_RED_PITAYA_BIST_HPP
#define KOHERON_RED_PITAYA_BIST_HPP
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
// RPC facade. Hardware and measurement implementation stays out of generated code.
class Bist
{
  public:
    Bist();
    ~Bist();
    bool start(uint32_t analog_mask);
    void cancel();
    bool running();
    double get_boot_seconds();
    std::string get_report();
    std::string get_scorecard();
    std::string get_log(uint32_t cursor);
    uint32_t ping(uint32_t value);
    std::vector<uint32_t> benchmark_reply(uint32_t words);
    bool record_startup(double network_seconds, double server_seconds, double boot_seconds);
    bool record_lifecycle(double restart_seconds, bool recovered);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
#endif
