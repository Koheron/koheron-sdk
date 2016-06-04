/// At93c46dscope driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_AT93C46D_HPP__
#define __DRIVERS_CORE_AT93C46D_HPP__

#include <vector>

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/wr_register.hpp>
#include <drivers/addresses.hpp>

// http://www.atmel.com/images/Atmel-5193-SEEPROM-AT93C46D-Datasheet.pdf
#define WRITE_OPCODE 1
#define READ_OPCODE 2
#define ERASE_OPCODE 3

#define EWDS 0
#define WRAL 1
#define ERAL 2
#define EWEN 3

class At93c46d
{
  public:
    At93c46d(Klib::DevMem& dvm_);

    int Open();

    uint32_t read(uint32_t addr) {
        dvm.write32(config_map, SPI_IN_OFF, 0);
        dvm.write32(config_map, SPI_IN_OFF, (READ_OPCODE << 7) + (addr << 1) + 1);
        return dvm.read32(status_map, SPI_OUT_OFF); 
    }

    void write_enable() {
        dvm.write32(config_map, SPI_IN_OFF, 0);
        dvm.write32(config_map, SPI_IN_OFF, (EWEN << 5) + 1);
    };

    void erase(uint32_t data_in) {
        dvm.write32(config_map, SPI_IN_OFF, 0);
        dvm.write32(config_map, SPI_IN_OFF, (data_in << 16) + (WRAL << 5) + 1);
    };

    uint32_t write(uint32_t addr, uint32_t data_in) {
        dvm.write32(config_map, SPI_IN_OFF, 0);
        dvm.write32(config_map, SPI_IN_OFF, (data_in << 16) + (WRITE_OPCODE << 7) + (addr << 1) + 1);
        return dvm.read32(status_map, SPI_OUT_OFF);
    };

    void erase_all(uint32_t addr) {
        dvm.write32(config_map, SPI_IN_OFF, 0);
        dvm.write32(config_map, SPI_IN_OFF, (ERAL << 5) + 1);
    };

    uint32_t write_all(uint32_t addr, uint32_t data_in) {
        dvm.write32(config_map, SPI_IN_OFF, 0);
        dvm.write32(config_map, SPI_IN_OFF, (data_in << 16) + (WRITE_OPCODE << 7) + (addr << 1) + 1);
        return dvm.read32(status_map, SPI_OUT_OFF);
    };

    void erase_write_disable() {
        dvm.write32(config_map, SPI_IN_OFF, 0);
        dvm.write32(config_map, SPI_IN_OFF, (EWDS << 5) + 1);
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

    // Memory maps IDs:
    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    
}; // class At93c46d

#endif // __DRIVERS_CORE_AT93C46D_HPP__
