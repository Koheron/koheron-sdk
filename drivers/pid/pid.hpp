/// (c) Koheron

#ifndef __DRIVERS_PID_HPP__
#define __DRIVERS_PID_HPP__

#include <drivers/lib/memory_manager.hpp>
#include <drivers/lib/fifo_reader.hpp>
#include <drivers/memory.hpp>

#define FIFO_BUFF_SIZE 65536
#define SAMPLING_FREQ 125e6
#define POW_32 4294967296

constexpr float freq_factor = POW_32 / SAMPLING_FREQ;

class Pid
{
  public:
    Pid(MemoryManager& mm)
    : cfg(mm.get<mem::config>())
    , sts(mm.get<mem::status>())
    , fifo(mm)
    {}

    void set_cic_rate(uint32_t rate) {
        cfg.write<reg::cic_rate>(rate);
    }

    void set_dds_freq(float freq) {
        cfg.write<reg::dds>(uint32_t(freq * freq_factor));
    }

    /// @acq_period Sleeping time between two acquisitions (us)
    void fifo_start_acquisition(uint32_t acq_period) {fifo.start_acquisition(acq_period);}
    void fifo_stop_acquisition()                     {fifo.stop_acquisition();}
    uint32_t get_fifo_length()                       {return fifo.get_fifo_length();}
    uint32_t get_fifo_buffer_length()                {return fifo.get_buffer_length();}
    std::vector<uint32_t>& get_fifo_data()           {return fifo.get_data();}
    bool fifo_get_acquire_status()                   {return fifo.get_acquire_status();}

  private:
    Memory<mem::config>& cfg;
    Memory<mem::status>& sts;
    FIFOReader<mem::fifo, FIFO_BUFF_SIZE> fifo;
};

#endif // __DRIVERS_PID_HPP__
