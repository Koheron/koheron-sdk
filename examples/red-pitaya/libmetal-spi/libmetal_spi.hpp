#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// A single SPI controller in internal loopback mode. All RPC calls are serialized
// by the SDK; the implementation also synchronizes with libmetal's IRQ thread.
class LibmetalSpi {
  public:
    LibmetalSpi();
    ~LibmetalSpi();

    bool is_ready();
    std::string get_error();
    std::vector<uint8_t> transfer(std::vector<uint8_t> data, uint32_t timeout_ms);
    uint32_t get_interrupt_count();
    uint32_t get_last_duration_us();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
