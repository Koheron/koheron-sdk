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
    Common(DevMem& dvm_)
    : dvm(dvm_)
    {
        config_map = dvm.add_memory_map(CONFIG_ADDR, CONFIG_RANGE);
        status_map = dvm.add_memory_map(STATUS_ADDR, STATUS_RANGE, PROT_READ);
    }

    std::array<uint32_t, BITSTREAM_ID_SIZE> get_bitstream_id();

    uint64_t get_dna();

    void set_led(uint32_t value) {dvm.write(config_map, LED_OFF, value);}
    uint32_t get_led()           {return dvm.read(config_map, LED_OFF);}

    void ip_on_leds();

    void init() {
        ip_on_leds();
        Init init(dvm);
        init.load_settings();
    };

  private:
    DevMem& dvm;

    MemMapID config_map;
    MemMapID status_map;

    std::array<uint32_t, BITSTREAM_ID_SIZE> bitstream_id;
};

#endif // __DRIVERS_COMMON_HPP__
