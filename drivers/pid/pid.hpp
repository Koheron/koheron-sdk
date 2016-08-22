/// Spectrum analyzer driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_PID_HPP__
#define __DRIVERS_CORE_PID_HPP__

#include <drivers/lib/fifo_reader.hpp>
#include <drivers/memory.hpp>

#define FIFO_BUFF_SIZE 65536
#define SAMPLING_FREQ 125e6
#define POW_32 4294967296

constexpr float freq_factor = POW_32 / SAMPLING_FREQ;

class Pid
{
  public:
    Pid(DevMem& dvm_)
    : dvm(dvm_)
    , cfg(dvm.get<CONFIG_MEM>())
    , sts(dvm.get<STATUS_MEM>())
    , fifo(dvm_)
    {}

    void set_cic_rate(uint32_t rate) {
        cfg.write<CIC_RATE_OFF>(rate);
    }

    void set_dds_freq(float freq) {
        cfg.write<DDS_OFF>(uint32_t(freq * freq_factor));
    }

    /// @acq_period Sleeping time between two acquisitions (us)
    void fifo_start_acquisition(uint32_t acq_period) {fifo.start_acquisition(acq_period);}
    void fifo_stop_acquisition()                     {fifo.stop_acquisition();}
    uint32_t get_fifo_length()                       {return fifo.get_fifo_length();}
    uint32_t get_fifo_buffer_length()                {return fifo.get_buffer_length();}
    std::vector<uint32_t>& get_fifo_data()           {return fifo.get_data();}
    bool fifo_get_acquire_status()                   {return fifo.get_acquire_status();}

  private:
    DevMem& dvm;
    MemoryMap<CONFIG_MEM>& cfg;
    MemoryMap<STATUS_MEM>& sts;
    FIFOReader<FIFO_MEM, FIFO_BUFF_SIZE> fifo;
}; // class Pid

#endif // __DRIVERS_CORE_PID_HPP__
