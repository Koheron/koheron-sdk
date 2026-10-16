#pragma once
#include <cstdint>

// A distinct SDK lock lets the measured driver serve RPCs and cancellation.
class FullCompletion
{
  public:
    bool wait(uint32_t timeout_ms);
};
