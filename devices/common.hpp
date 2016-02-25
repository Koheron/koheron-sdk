/// Common commands for all bitstreams
///
/// (c) Koheron

#ifndef __DRIVERS_COMMON_HPP__
#define __DRIVERS_COMMON_HPP__

#include <array>

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>
#include <drivers/addresses.hpp>

class Common
{
  public:
    Common(Klib::DevMem& dev_mem_);
    ~Common();

    //> \io_type WRITE
    //> \flag AT_INIT
    int Open();

    void Close();

    //> \io_type READ
    std::array<uint32_t, BITSTREAM_ID_SIZE> get_bitstream_id();

    //> \io_type READ
    uint64_t get_dna();

    //> \io_type WRITE
    void set_led(uint32_t value);

    //> \io_type READ
    uint32_t get_led();

    //> \io_type WRITE
    void ip_on_leds();

    enum Status {
        CLOSED,
        OPENED,
        FAILED
    };

    //> \is_failed
    bool IsFailed() const {return status == FAILED;}

  private:
    Klib::DevMem& dev_mem;
    int status;
    std::array<uint32_t, BITSTREAM_ID_SIZE> bitstream_id;

    // Memory maps IDs
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
};

#endif // __DRIVERS_COMMON_HPP__