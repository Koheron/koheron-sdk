/// FIFO driver
///
/// (c) Koheron

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf

#ifndef __SERVER_DRIVERS_FIFO_HPP__
#define __SERVER_DRIVERS_FIFO_HPP__

#include "server/runtime/services.hpp"
#include "server/context/memory_manager.hpp"

#include <chrono>

template <MemID fifo_mem>
class Fifo
{
  public:
    Fifo()
    : fifo(services::require<MemoryManager>().get<fifo_mem>())
    {}

    uint32_t occupancy() {
        return fifo.template read<RDFO>();
    }

    void reset() {
        fifo.template write<RDFR>(0x000000A5);
    }

    template<typename T = uint32_t>
    uint32_t read() {
        return fifo.template read<RDFD, T>();
    }

    uint32_t length() {
        return (fifo.template read<RLR>() & 0x3FFFFF) >> 2;
    }

    void wait_for_data(uint32_t n_pts, float fs_hz) {
        using namespace std::chrono_literals;
        const auto fifo_duration = 1s * n_pts / fs_hz;

        while (length() < n_pts) {
            std::this_thread::sleep_for(0.55 * fifo_duration);
        }
    }

  private:
    // AXI FIFO MM-S byte offsets (PG080)
    static constexpr uint32_t ISR  = 0x00; // Interrupt Status (RW1C)
    static constexpr uint32_t IER  = 0x04; // Interrupt Enable
    static constexpr uint32_t TDFR = 0x08; // TX FIFO Reset (W: 0xA5)
    static constexpr uint32_t RDFR = 0x18; // RX FIFO Reset (W: 0xA5)
    static constexpr uint32_t RDFO = 0x1C; // RX FIFO Occupancy (words)
    static constexpr uint32_t RDFD = 0x20; // RX Data (32-bit)
    static constexpr uint32_t RLR = 0x24;  // Receive length
    static constexpr uint32_t SRR  = 0x28; // Stream Reset (W: 0xA5)

    Memory<fifo_mem>& fifo;
};

#endif // __SERVER_DRIVERS_FIFO_HPP__