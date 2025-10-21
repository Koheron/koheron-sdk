/// demodulator driver
///
/// (c) Koheron

#ifndef __DRIVERS_DEMODULATOR_HPP__
#define __DRIVERS_DEMODULATOR_HPP__

#include "server/runtime/syslog.hpp"
#include "server/hardware/memory_manager.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <chrono>
#include <vector>
#include <thread>

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf
namespace Fifo_regs {
    constexpr uint32_t rdfr = 0x18;
    constexpr uint32_t rdfo = 0x1C;
    constexpr uint32_t rdfd = 0x20;
    constexpr uint32_t rlr = 0x24;
}

constexpr uint32_t fifo_buff_size = 8192 * 256;

class Demodulator
{
  public:
    Demodulator()
    : adc_fifo_map(hw::get_memory<mem::adc_fifo>())
    {
        start_fifo_acquisition();
    }

    ~Demodulator() {
        fifo_acquisition_started = false;
        fifo_thread.join();
    }

    void reset_fifo() {adc_fifo_map.write<Fifo_regs::rdfr>(0x000000A5);}
    int32_t read_fifo() {return adc_fifo_map.read<Fifo_regs::rdfd>();}
    uint32_t get_fifo_length() {return (adc_fifo_map.read<Fifo_regs::rlr>() & 0x3FFFFF) >> 2;}

    std::vector<int32_t>& get_vector(uint32_t n_pts) {
        last_buffer_vect.resize(n_pts);
        std::lock_guard<std::mutex> lock(mutex);
        uint32_t start_idx = fifo_buff_idx - (fifo_buff_idx % 2);
        for (uint32_t i = 0; i < n_pts; i++) {
            last_buffer_vect[n_pts - 1 - i] = fifo_buffer[(start_idx - 1 - i) % fifo_buff_size];
        }
        return last_buffer_vect;
    }

    void start_fifo_acquisition();

  private:
    hw::Memory<mem::adc_fifo>& adc_fifo_map;

    std::mutex mutex;

    std::atomic<bool> fifo_acquisition_started{false};
    std::atomic<uint32_t> fifo_buff_idx{0};
    std::array<int32_t, fifo_buff_size> fifo_buffer;

    std::vector<int32_t> last_buffer_vect;
    std::thread fifo_thread;
    void fifo_acquisition_thread();
};

inline void Demodulator::start_fifo_acquisition() {
    if (! fifo_acquisition_started) {
        fifo_buffer.fill(0);
        fifo_thread = std::thread{&Demodulator::fifo_acquisition_thread, this};
        fifo_thread.detach();
    }
}

inline void Demodulator::fifo_acquisition_thread()
{
    using namespace std::chrono_literals;
    fifo_acquisition_started = true;
    while (fifo_acquisition_started) {
        {
            std::lock_guard lock(mutex);
            const uint32_t n_pts = get_fifo_length();
            logf("fifo_length: {}\n", n_pts);

            for (size_t i = 0; i < n_pts; i++) {
                fifo_buffer[fifo_buff_idx] = read_fifo();
                fifo_buff_idx = (fifo_buff_idx + 1) % fifo_buff_size;
            }
        }

        std::this_thread::sleep_for(10ms);
    }
}

#endif // __DRIVERS_DEMODULATOR_HPP__