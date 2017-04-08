/// Pulse driver
///
/// (c) Koheron

#ifndef __DRIVERS_DECIMATOR_HPP__
#define __DRIVERS_DECIMATOR_HPP__

#include <context.hpp>

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf
namespace Fifo_regs {
    constexpr uint32_t rdfr = 0x18;
    constexpr uint32_t rdfo = 0x1C;
    constexpr uint32_t rdfd = 0x20;
    constexpr uint32_t rlr = 0x24;
}

constexpr uint32_t ARR_SIZE = 8192;

class Decimator
{
  public:
    Decimator(Context& ctx)
    : ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , adc_fifo_map(ctx.mm.get<mem::adc_fifo>())
    {}

    // Adc FIFO

    uint32_t get_fifo_occupancy() {
        return adc_fifo_map.read<Fifo_regs::rdfo>();
    }

    void reset_fifo() {
        adc_fifo_map.write<Fifo_regs::rdfr>(0x000000A5);
    }

    uint32_t read_fifo() {
        return adc_fifo_map.read<Fifo_regs::rdfd>();
    }

    uint32_t get_fifo_length() {
        return (adc_fifo_map.read<Fifo_regs::rlr>() & 0x3FFFFF) >> 2;
    }

    void wait_for(uint32_t n_pts) {
        do {} while (get_fifo_length() < n_pts);
    }

    auto& read_adc() {
        wait_for(ARR_SIZE);
        for (unsigned int i=0; i < ARR_SIZE; i++) {
            adc_data[i] = read_fifo();
        }
        return adc_data;
    }

  private:

    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::adc_fifo>& adc_fifo_map;

    std::array<uint32_t, ARR_SIZE> adc_data;
};

#endif // __DRIVERS_PULSE_HPP__
