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
    , cfg(dvm.get<CONFIG_MEM>())
    , sts(dvm.get<STATUS_MEM>())
    {}

    std::array<uint32_t, BITSTREAM_ID_SIZE> get_bitstream_id();

    uint64_t get_dna();

    void set_led(uint32_t value) {
    	cfg.write<LED_OFF>(value);
    }

    uint32_t get_led() {
    	return cfg.read<LED_OFF>();
    }

    void ip_on_leds();

    void init() {
        ip_on_leds();
        Init init(dvm);
        init.load_settings();
    };

    void cfg_write(uint32_t offset, uint32_t value) {
        cfg.write_offset(offset, value);
    }

    uint32_t cfg_read(uint32_t offset) {
        return cfg.read_offset(offset);
    }

    uint32_t sts_read(uint32_t offset) {
        return sts.read_offset(offset);
    }

    std::string get_instrument_config() {
        return CFG_JSON;
    }

  private:
    DevMem& dvm;
    MemoryMap<CONFIG_MEM>& cfg;
    MemoryMap<STATUS_MEM>& sts;

    std::array<uint32_t, BITSTREAM_ID_SIZE> bitstream_id;
};

#endif // __DRIVERS_COMMON_HPP__
