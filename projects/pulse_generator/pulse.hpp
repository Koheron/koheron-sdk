/// Pulse driver
///
/// (c) Koheron

#ifndef __DRIVERS_PULSE_HPP__
#define __DRIVERS_PULSE_HPP__

#include <drivers/lib/memory_manager.hpp>
#include <drivers/memory.hpp>

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf
#define FIFO_RDFR_OFF 0x18
#define FIFO_RDFO_OFF 0x1C
#define FIFO_RDFD_OFF 0x20
#define FIFO_RLR_OFF 0x24

#define DAC_SIZE mem::dac_range/sizeof(uint32_t)

class Pulse
{
  public:
    Pulse(MemoryManager& mm)
    : cfg(mm.get<mem::config>())
    , sts(mm.get<mem::status>())
    , adc_fifo_map(mm.get<mem::adc_fifo>())
    , dac_map(mm.get<mem::dac>())
    {}

    // Trigger

    void trig_pulse() {
        cfg.set_bit<reg::trigger, 0>();
        cfg.clear_bit<reg::trigger, 0>();
    }

    // Pulse generator

    void set_pulse_generator(uint32_t pulse_width, uint32_t pulse_period) {
        cfg.write<reg::pulse_width>(pulse_width);
        cfg.write<reg::pulse_period>(pulse_period);
    }

    void set_dac_data(const std::array<uint32_t, DAC_SIZE>& data) {
        dac_map.write_array(data);
    }

    uint32_t get_count() {
        return sts.read<reg::count>();
    }

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

    std::vector<uint32_t>& get_next_pulse(uint32_t n_pts) {
        adc_data.resize(n_pts);

        if (n_pts == 0)
            return adc_data;

        wait_for(1);
        uint32_t data = read_fifo();

        // Wait for the beginning of a pulse
        while ((data & (1 << 15)) != (1 << 15)) {
            wait_for(1);
            data = read_fifo();
        }

        adc_data[0] = data;
        wait_for(n_pts -1);

        for (unsigned int i=1; i < n_pts; i++)
            adc_data[i] = read_fifo();
        return adc_data;
    }

  private:
    Memory<mem::config>& cfg;
    Memory<mem::status>& sts;
    Memory<mem::adc_fifo>& adc_fifo_map;
    Memory<mem::dac>& dac_map;

    std::vector<uint32_t> adc_data;
};

#endif // __DRIVERS_PULSE_HPP__
