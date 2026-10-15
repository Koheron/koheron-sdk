#ifndef KOHERON_RED_PITAYA_BIST_COMPLETION_HPP
#define KOHERON_RED_PITAYA_BIST_COMPLETION_HPP
#include <cstdint>

// Waiting must not hold Bist's SDK driver lock: the workload calls Bist over RPC.
class BistCompletion
{
  public:
    bool wait(uint32_t timeout_ms);
};
#endif
