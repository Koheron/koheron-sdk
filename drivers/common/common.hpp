/// Common commands for all bitstreams
///
/// (c) Koheron

#ifndef __DRIVERS_COMMON_HPP__
#define __DRIVERS_COMMON_HPP__

#include <array>

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/wr_register.hpp>
#include <drivers/addresses.hpp>
#include <drivers/init.hpp>

class Common
{
  public:
    Common(Klib::DevMem& dvm_);

    int Open();

    std::array<uint32_t, BITSTREAM_ID_SIZE> get_bitstream_id();

    uint64_t get_dna();

    void set_led(uint32_t value) {
        dvm.write32(config_map, LED_OFF, value);
    }

    uint32_t get_led() {
        return dvm.read32(config_map, LED_OFF);
    }

    void ip_on_leds();

    void init() {
        ip_on_leds();
        Init init(dvm);
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
    Klib::DevMem& dvm;
    int status;
    std::array<uint32_t, BITSTREAM_ID_SIZE> bitstream_id;

    // Memory maps IDs
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
};

#endif // __DRIVERS_COMMON_HPP__