#ifndef __DRIVERS_EEPROM_HPP__
#define __DRIVERS_EEPROM_HPP__

#include <vector>
#include <thread>
#include <chrono>

#include <context.hpp>

// EEPROM instructions
// http://www.atmel.com/images/Atmel-5193-SEEPROM-AT93C46D-Datasheet.pdf
namespace Eeprom_instr {
    constexpr uint32_t write = 1 << 2; // Write memory location
    constexpr uint32_t read = 2 << 2; // Reads data stored in memory at specified address
    constexpr uint32_t erase = 3 << 2; // Erases memory location
    constexpr uint32_t ewds = 0; // Disable all programming instructions
    constexpr uint32_t wral = 1; // Disable all programming instructions
    constexpr uint32_t eral = 2; // Erase all memory locations
    constexpr uint32_t ewen = 3; // Write Enable, must precede all programming modes
}

class Eeprom
{
  public:
    Eeprom(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    {
        using namespace std::chrono_literals;
        write_enable();
        std::this_thread::sleep_for(10ms);
    }

    uint32_t read(uint32_t addr) {
        using namespace std::chrono_literals;
        ctl.write<reg::eeprom_ctl>((Eeprom_instr::read << 5) + (addr << 1));
        ctl.set_bit<reg::eeprom_ctl, 0>();
        std::this_thread::sleep_for(100us);
        return sts.read<reg::eeprom_sts>();
    }

    void write_enable() {
        ctl.write<reg::eeprom_ctl>(Eeprom_instr::ewen << 5);
        ctl.set_bit<reg::eeprom_ctl, 0>();
    }

    void erase(uint32_t addr) {
        ctl.write<reg::eeprom_ctl>((Eeprom_instr::erase << 5) + (addr << 1));
        ctl.set_bit<reg::eeprom_ctl, 0>();
    }

    void write(uint32_t addr, uint32_t data_in) {
        ctl.write<reg::eeprom_ctl>((data_in << 16) + (Eeprom_instr::write << 5) + (addr << 1));
        ctl.set_bit<reg::eeprom_ctl, 0>();
    }

    void erase_all() {
        ctl.write<reg::eeprom_ctl>(Eeprom_instr::eral << 5);
        ctl.set_bit<reg::eeprom_ctl, 0>();
    }

    void write_all(uint32_t data_in) {
        ctl.write<reg::eeprom_ctl>((data_in << 16) + (Eeprom_instr::wral << 5));
        ctl.set_bit<reg::eeprom_ctl, 0>();
    }

    void erase_write_disable() {
        ctl.write<reg::eeprom_ctl>(Eeprom_instr::ewds << 5);
        ctl.set_bit<reg::eeprom_ctl, 0>();
    }

  private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
};

#endif // __DRIVERS_EEPROM_HPP__