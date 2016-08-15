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
        config_map = dvm.add_memory_map(CONFIG_ADDR, CONFIG_RANGE);
        status_map = dvm.add_memory_map(STATUS_ADDR, STATUS_RANGE, PROT_READ);
    }

    uint32_t read(uint32_t addr) {
        dvm.write<SPI_IN_OFF>(config_map, (READ_OPCODE << 7) + (addr << 1));
        dvm.set_bit<SPI_IN_OFF, 0>(config_map);
        std::this_thread::sleep_for(100us);
        return dvm.read<SPI_OUT_OFF>(status_map);
    }

    void write_enable() {
        dvm.write<SPI_IN_OFF>(config_map, (EWEN << 5));
        dvm.set_bit<SPI_IN_OFF, 0>(config_map);
    }

    void erase(uint32_t addr) {
        dvm.write<SPI_IN_OFF>(config_map, (ERASE_OPCODE << 7) + (addr << 1));
        dvm.set_bit<SPI_IN_OFF, 0>(config_map);
    }

    void write(uint32_t addr, uint32_t data_in) {
        dvm.write<SPI_IN_OFF>(config_map, (data_in << 16) + (WRITE_OPCODE << 7) + (addr << 1));
        dvm.set_bit<SPI_IN_OFF, 0>(config_map);
    }

    void erase_all() {
        dvm.write<SPI_IN_OFF>(config_map, (ERAL << 5));
        dvm.set_bit<SPI_IN_OFF, 0>(config_map);
    }

    void write_all(uint32_t data_in) {
        dvm.write<SPI_IN_OFF>(config_map, (data_in << 16) + (WRAL << 5));
        dvm.set_bit<SPI_IN_OFF, 0>(config_map);
    }

    void erase_write_disable() {
        dvm.write<SPI_IN_OFF>(config_map, (EWDS << 5));
        dvm.set_bit<SPI_IN_OFF, 0>(config_map);
    }

  private:
    DevMem& dvm;

    MemMapID config_map;
    MemMapID status_map;
}; // class Eeprom

#endif // __DRIVERS_CORE_EEPROM_HPP__
