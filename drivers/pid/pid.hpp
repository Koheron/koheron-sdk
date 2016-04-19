/// Spectrum analyzer driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_PID_HPP__
#define __DRIVERS_CORE_PID_HPP__

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>
#include <drivers/addresses.hpp>

#include "lib/fifo_reader.hpp"

#define FIFO_BUFF_SIZE 65536

class Pid
{
  public:
    Pid(Klib::DevMem& dev_mem_);

    int Open();

    /// @acq_period Sleeping time between two acquisitions (us)
    void fifo_start_acquisition(uint32_t acq_period);
    void fifo_stop_acquisition();
    uint32_t get_fifo_length();
    uint32_t get_fifo_buffer_length();
    std::vector<uint32_t>& get_fifo_data();
    bool fifo_get_acquire_status();

    enum Status {
        CLOSED,
        OPENED,
        FAILED
    };

    #pragma tcp-server is_failed
    bool IsFailed() const {return status == FAILED;}

  private:
    Klib::DevMem& dev_mem;
    int status;

    // Memory maps IDs:
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID fifo_map;

    FIFOReader<FIFO_BUFF_SIZE> fifo;
   
}; // class Pid

#endif // __DRIVERS_CORE_PID_HPP__
