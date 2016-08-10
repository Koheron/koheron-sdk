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
    {
        config_map = dvm.AddMemoryMap(CONFIG_ADDR, CONFIG_RANGE);
        status_map = dvm.AddMemoryMap(STATUS_ADDR, STATUS_RANGE, PROT_READ);
    }

    uint32_t read(uint32_t addr) {
        dvm.write32(config_map, SPI_IN_OFF, (READ_OPCODE << 7) + (addr << 1));
        dvm.set_bit(config_map, SPI_IN_OFF, 0);
        std::this_thread::sleep_for(100us);
        return dvm.read32(status_map, SPI_OUT_OFF);
    }

    void write_enable() {
        dvm.write32(config_map, SPI_IN_OFF, (EWEN << 5));
        dvm.set_bit(config_map, SPI_IN_OFF, 0);
    }

    void erase(uint32_t addr) {
        dvm.write32(config_map, SPI_IN_OFF, (ERASE_OPCODE << 7) + (addr << 1));
        dvm.set_bit(config_map, SPI_IN_OFF, 0);
    }

    void write(uint32_t addr, uint32_t data_in) {
        dvm.write32(config_map, SPI_IN_OFF, (data_in << 16) + (WRITE_OPCODE << 7) + (addr << 1));
        dvm.set_bit(config_map, SPI_IN_OFF, 0);
    }

    void erase_all() {
        dvm.write32(config_map, SPI_IN_OFF, (ERAL << 5));
        dvm.set_bit(config_map, SPI_IN_OFF, 0);
    }

    void write_all(uint32_t data_in) {
        dvm.write32(config_map, SPI_IN_OFF, (data_in << 16) + (WRAL << 5));
        dvm.set_bit(config_map, SPI_IN_OFF, 0);
    }

    void erase_write_disable() {
        dvm.write32(config_map, SPI_IN_OFF, (EWDS << 5));
        dvm.set_bit(config_map, SPI_IN_OFF, 0);
    }

  private:
    DevMem& dvm;

    MemMapID config_map;
    MemMapID status_map;
}; // class Eeprom

#endif // __DRIVERS_CORE_EEPROM_HPP__
