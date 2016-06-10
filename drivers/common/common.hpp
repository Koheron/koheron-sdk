/// Common commands for all bitstreams
///
/// (c) Koheron

#ifndef __DRIVERS_COMMON_HPP__
#define __DRIVERS_COMMON_HPP__

#include <array>

#include <drivers/lib/dev_mem.hpp>
#include <drivers/addresses.hpp>
#include <drivers/init/init.hpp>

class Common
{
  public:
    Common(Klib::DevMem& dvm_)
    : dvm(dvm_)
    {
        config_map = dvm.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
        status_map = dvm.AddMemoryMap(STATUS_ADDR, STATUS_RANGE, PROT_READ);
    }

    int Open() {return dvm.is_ok() ? 0 : -1;}

    std::array<uint32_t, BITSTREAM_ID_SIZE> get_bitstream_id();

    uint64_t get_dna();

    void set_led(uint32_t value) {dvm.write32(config_map, LED_OFF, value);}
    uint32_t get_led()           {return dvm.read32(config_map, LED_OFF);}

    void ip_on_leds();

    void init() {
        ip_on_leds();
        Init init(dvm);
        init.load_settings();
    };

    #pragma tcp-server is_failed
    bool IsFailed() const {return dvm.IsFailed();}

  private:
    Klib::DevMem& dvm;

    Klib::MemMapID config_map;
    Klib::MemMapID status_map;

    std::array<uint32_t, BITSTREAM_ID_SIZE> bitstream_id;
};

#endif // __DRIVERS_COMMON_HPP__
