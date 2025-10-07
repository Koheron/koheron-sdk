/// FIFO driver
///
/// (c) Koheron

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf

#ifndef __SERVER_DRIVERS_FIFO_HPP__
#define __SERVER_DRIVERS_FIFO_HPP__

#include "server/context/context.hpp"

#include <chrono>

template <size_t fifo_mem>
class Fifo
{
  public:
    Fifo(Context& ctx_)
    : ctx(ctx_)
    , fifo(ctx.mm.get<fifo_mem>())
    {}

    uint32_t occupancy() {
        return fifo.template read<rdfo>();
    }

    void reset() {
        fifo.template write<rdfr>(0x000000A5);
    }

    template<typename T = uint32_t>
    uint32_t read() {
        return fifo.template read<rdfd, T>();
    }

    uint32_t length() {
        return (fifo.template read<rlr>() & 0x3FFFFF) >> 2;
    }

    void wait_for_data(uint32_t n_pts, float fs_hz) {
        using namespace std::chrono_literals;
        const auto fifo_duration = 1s * n_pts / fs_hz;

        while (length() < n_pts) {
            std::this_thread::sleep_for(0.55 * fifo_duration);
        }
    }

  private:
    static constexpr uint32_t rdfr = 0x18;
    static constexpr uint32_t rdfo = 0x1C;
    static constexpr uint32_t rdfd = 0x20;
    static constexpr uint32_t rlr = 0x24;

    Context& ctx;
    Memory<fifo_mem>& fifo;
};

#endif // __SERVER_DRIVERS_FIFO_HPP__