/// Common commands for all bitstreams
///
/// (c) Koheron

#ifndef __DRIVERS_COMMON_HPP__
#define __DRIVERS_COMMON_HPP__

#include <array>

#include <drivers/dev_mem.hpp>
#include <drivers/wr_register.hpp>
#include <drivers/addresses.hpp>
#include <drivers/init.hpp>

class Common
{
  public:
    Common(Klib::DevMem& dev_mem_);
    ~Common();

    int Open();

    #pragma tcp-server exclude
    void Close();

    std::array<uint32_t, BITSTREAM_ID_SIZE> get_bitstream_id();

    uint64_t get_dna();
    void set_led(uint32_t value);
    uint32_t get_led();
    void ip_on_leds();
    
    void load_settings() {
        Init init(dev_mem);
        init.load_settings();
    };

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
    std::array<uint32_t, BITSTREAM_ID_SIZE> bitstream_id;

    // Memory maps IDs
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
};

#endif // __DRIVERS_COMMON_HPP__