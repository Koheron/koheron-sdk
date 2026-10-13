/// ADC DAC BRAM driver
///
/// (c) Koheron

#ifndef __DRIVERS_ADC_DAC_BRAM_HPP__
#define __DRIVERS_ADC_DAC_BRAM_HPP__

#include "server/hardware/memory_manager.hpp"
#include "server/runtime/syslog.hpp"
#include "boards/alpha250/drivers/clock-generator.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <array>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>

class AdcDacBram
{
  public:
    static constexpr uint32_t dac_size = mem::dac_range / sizeof(uint32_t);
    static constexpr uint32_t adc_size = mem::adc_range / sizeof(uint32_t);

    void trigger_acquisition() {
        std::lock_guard clock_lock(clock_cfg::sampling_mutex);
        (void)acquire();
    }

    uint32_t get_dac_size() {
        return dac_size;
    }

    uint32_t get_adc_size() {
        return adc_size;
    }

    void set_dac_data(const std::array<uint32_t, AdcDacBram::dac_size>& data) {
        hw::get_memory<mem::dac>().write_array(data);
    }

    auto get_adc() {
        std::lock_guard clock_lock(clock_cfg::sampling_mutex);
        if (!acquire()) return std::array<uint32_t, adc_size>{};
        return hw::get_memory<mem::adc>().read_array<uint32_t, adc_size>();
    }

    // Appended RPC: atomic validity + data, with legacy command IDs unchanged.
    auto get_adc_checked() {
        std::lock_guard clock_lock(clock_cfg::sampling_mutex);
        auto data = get_adc();
        return std::tuple{capture_valid, std::move(data)};
    }

  private:
    bool capture_valid = false;

    // Callers hold sampling_mutex through trigger, completion and readout.
    bool acquire() {
        capture_valid = false;
        auto& ctl = hw::get_memory<mem::control>();
        auto& sts = hw::get_memory<mem::status>();
        const auto previous = sts.read<reg::capture_complete>() & 1U;
        ctl.set_bit<reg::trig, 0>();
        (void)ctl.read<reg::trig>();
        ctl.clear_bit<reg::trig, 0>();
        (void)ctl.read<reg::trig>();

        // Even at 100 MS/s the boundary wait plus capture is under 1.4 ms.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
        while ((sts.read<reg::capture_complete>() & 1U) == previous) {
            if (std::chrono::steady_clock::now() >= deadline) {
                log<ERROR>("BRAM acquisition timed out\n");
                return false;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        std::atomic_thread_fence(std::memory_order_seq_cst);
        capture_valid = true;
        return true;
    }
}; // class AdcDacBram

#endif // __DRIVERS_ADC_DAC_BRAM_HPP__
