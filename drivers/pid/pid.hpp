/// Spectrum analyzer driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_PID_HPP__
#define __DRIVERS_CORE_PID_HPP__

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/fifo_reader.hpp>
#include <drivers/addresses.hpp>

#define FIFO_BUFF_SIZE 65536

class Pid
{
  public:
    Pid(Klib::DevMem& dvm_)
    : dvm(dvm_)
    {
        config_map = dvm.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
        status_map = dvm.AddMemoryMap(STATUS_ADDR, STATUS_RANGE, Klib::MemoryMap::READ_ONLY);
        fifo_map = dvm.AddMemoryMap(FIFO_ADDR, FIFO_RANGE);
        fifo.set_address(dvm.GetBaseAddr(fifo_map));
    }

    int Open() {return dvm.is_ok() ? 0 : 1;}

    /// @acq_period Sleeping time between two acquisitions (us)
    void fifo_start_acquisition(uint32_t acq_period) {fifo.start_acquisition(acq_period);}
    void fifo_stop_acquisition()                     {fifo.stop_acquisition();}
    uint32_t get_fifo_length()                       {return fifo.get_fifo_length();}
    uint32_t get_fifo_buffer_length()                {return fifo.get_buffer_length();}
    std::vector<uint32_t>& get_fifo_data()           {return fifo.get_data();}
    bool fifo_get_acquire_status()                   {return fifo.get_acquire_status();}

    #pragma tcp-server is_failed
    bool IsFailed() const {return dvm.IsFailed();}

  private:
    Klib::DevMem& dvm;

    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID fifo_map;

    FIFOReader<FIFO_BUFF_SIZE> fifo;
}; // class Pid

#endif // __DRIVERS_CORE_PID_HPP__
