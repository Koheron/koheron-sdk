/// Spectrum analyzer driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_PID_HPP__
#define __DRIVERS_CORE_PID_HPP__

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>
#include <drivers/addresses.hpp>

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf
#define RDFR_OFF 0x18
#define RDFO_OFF 0x1C
#define RDFD_OFF 0x20
#define RLR_OFF 0x24

class Pid
{
  public:
    Pid(Klib::DevMem& dev_mem_);

    int Open();

    uint32_t get_fifo_occupancy();
    uint32_t get_fifo_length();
    void reset_fifo();
    std::vector<uint32_t>& get_fifo_data(uint32_t n_pts);

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

    std::vector<uint32_t> fifo_data;
   
}; // class Pid

#endif // __DRIVERS_CORE_PID_HPP__
