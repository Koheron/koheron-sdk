/// Eeprom driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_EEPROM_HPP__
#define __DRIVERS_CORE_EEPROM_HPP__

#include <vector>
#include <thread>
#include <chrono>

#include <drivers/lib/dev_mem.hpp>
#include <drivers/addresses.hpp>

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
    Eeprom(DevMem& dvm_)
    : dvm(dvm_)
    , cfg(dvm.get<CONFIG_MEM>())
    , sts(dvm.get<STATUS_MEM>())
    {}

    uint32_t read(uint32_t addr) {
        cfg.write<SPI_IN_OFF>((READ_OPCODE << 7) + (addr << 1));
        cfg.set_bit<SPI_IN_OFF, 0>();
        std::this_thread::sleep_for(100us);
        return sts.read<SPI_OUT_OFF>();
    }

    void write_enable() {
        cfg.write<SPI_IN_OFF>(EWEN << 5);
        cfg.set_bit<SPI_IN_OFF, 0>();
    }

    void erase(uint32_t addr) {
        cfg.write<SPI_IN_OFF>((ERASE_OPCODE << 7) + (addr << 1));
        cfg.set_bit<SPI_IN_OFF, 0>();
    }

    void write(uint32_t addr, uint32_t data_in) {
        cfg.write<SPI_IN_OFF>((data_in << 16) + (WRITE_OPCODE << 7) + (addr << 1));
        cfg.set_bit<SPI_IN_OFF, 0>();
    }

    void erase_all() {
        cfg.write<SPI_IN_OFF>(ERAL << 5);
        cfg.set_bit<SPI_IN_OFF, 0>();
    }

    void write_all(uint32_t data_in) {
        cfg.write<SPI_IN_OFF>((data_in << 16) + (WRAL << 5));
        cfg.set_bit<SPI_IN_OFF, 0>();
    }

    void erase_write_disable() {
        cfg.write<SPI_IN_OFF>(EWDS << 5);
        cfg.set_bit<SPI_IN_OFF, 0>();
    }

  private:
    DevMem& dvm;
    MemoryMap<CONFIG_MEM>& cfg;
    MemoryMap<STATUS_MEM>& sts;
}; // class Eeprom

#endif // __DRIVERS_CORE_EEPROM_HPP__
