#pragma once
#include "server/hardware/memory_manager.hpp"
#include <chrono>
#include <thread>

class DmaS2MM {
    uint32_t address = 0;
    uint32_t length = 0;
    uint32_t x_count = 0, y_count = 0;
  public:
    void start_transfer(uint32_t address_, uint32_t length_) { address = address_; length = length_; }
    template<typename Duration> bool wait_for_transfer_checked(Duration) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const bool y = hw::selected_input.load() != 0;
        auto& count = y ? y_count : x_count;
        auto& ram = hw::get_memory<mem::ram>();
        for (uint32_t i = 0; i < length / sizeof(int32_t); ++i) {
            ram.data[address / sizeof(int32_t) + i] = count * 8192 + i + (y ? 100 : 0);
        }
        ++count;
        return true;
    }
};
