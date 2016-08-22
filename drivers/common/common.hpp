/// Common commands for all bitstreams
///
/// (c) Koheron

#ifndef __DRIVERS_COMMON_HPP__
#define __DRIVERS_COMMON_HPP__

#include <array>

#include <drivers/lib/memory_manager.hpp>
#include <drivers/init/init.hpp>
#include <drivers/memory.hpp>

class Common
{
  public:
    Common(MemoryManager& mm_)
    : mm(mm_)
    , cfg(mm.get<mem::config>())
    , sts(mm.get<mem::status>())
    {}

    std::array<uint32_t, prm::bitstream_id_size> get_bitstream_id();

    uint64_t get_dna();

    void set_led(uint32_t value) {
        cfg.write<reg::led>(value);
    }

    uint32_t get_led() {
        return cfg.read<reg::led>();
    }

    void ip_on_leds();

    void init() {
        ip_on_leds();
        Init init(mm);
        init.load_settings();
    };

    void cfg_write(uint32_t offset, uint32_t value) {
        cfg.write_offset(offset, value);
    }

    uint32_t cfg_read(uint32_t offset) {
        return cfg.read_offset(offset);
    }

    auto& cfg_read_all() {
        return cfg.read_array<uint32_t, mem::config_range/4>();
    }

    auto& sts_read_all() {
        return sts.read_array<uint32_t, mem::status_range/4>();
    }

    uint32_t sts_read(uint32_t offset) {
        return sts.read_offset(offset);
    }

    std::string get_instrument_config() {
        return CFG_JSON;
    }

  private:
    MemoryManager& mm;
    MemoryMap<mem::config>& cfg;
    MemoryMap<mem::status>& sts;

    std::array<uint32_t, prm::bitstream_id_size> bitstream_id;
};

#endif // __DRIVERS_COMMON_HPP__
