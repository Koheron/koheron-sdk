/// Pulse driver
///
/// (c) Koheron

#ifndef __DRIVERS_DECIMATOR_HPP__
#define __DRIVERS_DECIMATOR_HPP__

#include <drivers/lib/memory_manager.hpp>
#include <drivers/memory.hpp>

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf
#define FIFO_RDFR_OFF 0x18
#define FIFO_RDFO_OFF 0x1C
#define FIFO_RDFD_OFF 0x20
#define FIFO_RLR_OFF 0x24

#define DAC_SIZE mem::dac_range/sizeof(uint32_t)

#define ARR_SIZE 8192

class Decimator
{
  public:
    Decimator(MemoryManager& mm)
    : cfg(mm.get<mem::config>())
    , sts(mm.get<mem::status>())
    , adc_fifo_map(mm.get<mem::adc_fifo>())
    {}

    // Adc FIFO

    uint32_t get_fifo_occupancy() {
        return adc_fifo_map.read<FIFO_RDFO_OFF>();
    }

    void reset_fifo() {
        adc_fifo_map.write<FIFO_RDFR_OFF>(0x000000A5);
    }

    uint32_t read_fifo() {
        return adc_fifo_map.read<FIFO_RDFD_OFF>();
    }

    uint32_t get_fifo_length() {
        return (adc_fifo_map.read<FIFO_RLR_OFF>() & 0x3FFFFF) >> 2;
    }

    void wait_for(uint32_t n_pts) {
        do {} while (get_fifo_length() < n_pts);
    }

    std::array<uint32_t, ARR_SIZE>& read_adc() {
        wait_for(ARR_SIZE);
        for (unsigned int i=0; i < ARR_SIZE; i++)
            adc_data[i] = read_fifo();
        return adc_data;
    }

  private:
    Memory<mem::config>& cfg;
    Memory<mem::status>& sts;
    Memory<mem::adc_fifo>& adc_fifo_map;

    std::array<uint32_t, ARR_SIZE> adc_data;
};

#endif // __DRIVERS_PULSE_HPP__
