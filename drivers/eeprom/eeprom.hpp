/// Eeprom driver
///
/// (c) Koheron

#ifndef __DRIVERS_EEPROM_HPP__
#define __DRIVERS_EEPROM_HPP__

#include <vector>
#include <thread>
#include <chrono>

#include <drivers/lib/memory_manager.hpp>
#include <drivers/memory.hpp>

// http://www.atmel.com/images/Atmel-5193-SEEPROM-AT93C46D-Datasheet.pdf
#define WRITE_OPCODE 1
#define READ_OPCODE 2
#define ERASE_OPCODE 3

#define EWDS 0
#define WRAL 1
#define ERAL 2
#define EWEN 3

using namespace std::chrono_literals;

class Eeprom
{
  public:
    Eeprom(MemoryManager& mm)
    : cfg(mm.get<mem::config>())
    , sts(mm.get<mem::status>())
    {}

    uint32_t read(uint32_t addr) {
        cfg.write<reg::spi_in>((READ_OPCODE << 7) + (addr << 1));
        cfg.set_bit<reg::spi_in, 0>();
        std::this_thread::sleep_for(100us);
        return sts.read<reg::spi_out>();
    }

    void write_enable() {
        cfg.write<reg::spi_in>(EWEN << 5);
        cfg.set_bit<reg::spi_in, 0>();
    }

    void erase(uint32_t addr) {
        cfg.write<reg::spi_in>((ERASE_OPCODE << 7) + (addr << 1));
        cfg.set_bit<reg::spi_in, 0>();
    }

    void write(uint32_t addr, uint32_t data_in) {
        cfg.write<reg::spi_in>((data_in << 16) + (WRITE_OPCODE << 7) + (addr << 1));
        cfg.set_bit<reg::spi_in, 0>();
    }

    void erase_all() {
        cfg.write<reg::spi_in>(ERAL << 5);
        cfg.set_bit<reg::spi_in, 0>();
    }

    void write_all(uint32_t data_in) {
        cfg.write<reg::spi_in>((data_in << 16) + (WRAL << 5));
        cfg.set_bit<reg::spi_in, 0>();
    }

    void erase_write_disable() {
        cfg.write<reg::spi_in>(EWDS << 5);
        cfg.set_bit<reg::spi_in, 0>();
    }

  private:
    MemoryMap<mem::config>& cfg;
    MemoryMap<mem::status>& sts;
}; // class Eeprom

#endif // __DRIVERS_EEPROM_HPP__
