#ifndef DAC_HEADER
#define DAC_HEADER "boards/alpha250/drivers/precision-dac.hpp"
#define EEPROM_HEADER "boards/alpha250/drivers/eeprom.hpp"
#endif
#include DAC_HEADER
#include EEPROM_HEADER
#include "server/hardware/memory_manager.hpp"
#include "server/runtime/driver_manager.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
std::array<float, 8> stored{20000, 0, 20000, 0, 20000, 0, 20000, 0};
bool fail_read = false, fail_write = false;
unsigned eeprom_writes = 0;
constexpr auto quiet_nan = std::numeric_limits<float>::quiet_NaN();
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
auto& memory = hw::control_fixture;
uint32_t code(unsigned channel) {
    return (memory.registers[1 + channel / 2] >> (16 * (channel % 2))) & 0xFFFF;
}
void no_data_writes() { check(memory.writes[1] == 0 && memory.writes[2] == 0, "invalid operation changed outputs"); }
}

// Compile the real DAC and EEPROM drivers, faking only I2C and MMIO services.
namespace hw {
I2cDev::I2cDev(std::string name) : devname(std::move(name)) {}
I2cDev::~I2cDev() = default;
I2cManager::I2cManager() : empty_i2cdev(std::make_unique<I2cDev>("fixture")) {}
I2cDev& I2cManager::get(const std::string&) { return *empty_i2cdev; }
int I2cDev::write(int32_t addr, const uint8_t* buffer, size_t size) {
    check(addr == 0x54 && ((unsigned(buffer[0]) << 8) | buffer[1]) == 0x100, "calibration address");
    if (size > 2) {
        ++eeprom_writes;
        if (fail_write) { errno = EIO; return -1; }
        check(size == sizeof(stored) + 2, "calibration size");
        std::memcpy(stored.data(), buffer + 2, sizeof(stored));
    }
    return static_cast<int>(size);
}
int I2cDev::read(int32_t addr, uint8_t* buffer, size_t size) {
    check(addr == 0x54 && size == sizeof(stored), "calibration read");
    std::memcpy(buffer, stored.data(), size);
    return fail_read ? -1 : static_cast<int>(size);
}
}

int main(int argc, char** argv) {
    check(argc == 2, "case required");
    const std::string_view test{argv[1]};
    auto bus = services::provide<hw::I2cManager>();
    auto eeprom = services::provide<Eeprom>();
    auto mm = services::provide<hw::MemoryManager>();
    auto dm = services::provide<rt::DriverManager>();
    PrecisionDac dac;
    if (test == "missing_calibration") {
        fail_read = true;
        dac.init();
        memory.writes.fill(0);
        dac.set_dac_value_volts(0, 1.0f);
        no_data_writes();
        check(dac.get_dac_values()[0] == 0.0f, "reported voltage without calibration");
        fail_read = false;
        dac.init();
        dac.set_dac_value_volts(0, 1.0f);
        check(code(0) == 20000, "calibration recovery");
        return 0;
    }
    dac.init();
    memory.writes.fill(0);
    if (test == "raw_bounds") {
        for (auto channel : {4u, 5u, std::numeric_limits<uint32_t>::max()}) { dac.set_dac_value(channel, 42); }
        for (auto invalid : {65536u, std::numeric_limits<uint32_t>::max()}) { dac.set_dac_value(0, invalid); }
        no_data_writes();
    } else if (test == "raw_pairs") {
        for (unsigned channel = 0; channel < 4; ++channel) {
            memory.writes.fill(0);
            dac.set_dac_value(channel, 0x1234 + channel);
            check(memory.writes[1 + channel / 2] == 1 && memory.writes[2 - channel / 2] == 0,
                  "wrote unaffected register pair");
            for (unsigned i = 0; i < 4; ++i) { check(code(i) == (i <= channel ? 0x1234 + i : 0), "neighbor overwritten"); }
        }
        dac.set_dac_value(3, 65535); check(code(3) == 65535, "maximum raw code rejected");
    } else if (test == "voltage") {
        for (unsigned channel = 0; channel < 4; ++channel) {
            for (const float volts : {-1.0f, 0.0f, 0.5f, 1.0f, 2.5f, 3.0f}) {
                dac.set_dac_value_volts(channel, volts);
                const float clamped = std::clamp(volts, 0.0f, 2.5f);
                check(code(channel) == static_cast<uint32_t>(std::round(clamped * 20000)), "normal conversion changed");
                check(dac.get_dac_values()[channel] == clamped, "reported voltage");
            }
        }
    } else if (test == "nonfinite") {
        const auto before = dac.get_dac_values();
        for (auto value : {quiet_nan, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
            dac.set_dac_value_volts(0, value);
        }
        dac.set_dac_value_volts(4, 1.0f);
        no_data_writes();
        check(dac.get_dac_values() == before, "invalid request changed reported voltage");
    } else if (test == "clipping") {
        auto coefficients = stored;
        coefficients[0] = 40000; coefficients[1] = -10;
        check(dac.set_calibration_coeffs(coefficients) == sizeof(coefficients), "save calibration");
        dac.set_dac_value_volts(0, 0); check(code(0) == 0, "negative code wrapped");
        dac.set_dac_value_volts(0, 2.5); check(code(0) == 65535, "high code wrapped");
        coefficients[1] = 0.5;
        check(dac.set_calibration_coeffs(coefficients) == sizeof(coefficients), "save half-step calibration");
        dac.set_dac_value_volts(0, 0); check(code(0) == 1, "half-step rounding changed");
    } else if (test == "rounding") {
        auto coefficients = stored;
        coefficients[0] = 65536; coefficients[1] = 0;
        check(dac.set_calibration_coeffs(coefficients) == sizeof(coefficients), "save rounding calibration");
        // Every half-code boundary and its nearest representable neighbors.
        for (unsigned whole = 0; whole < 65535; ++whole) {
            const float half = static_cast<float>(whole) + 0.5f;
            for (float value : {std::nextafter(half, 0.0f), half,
                                std::nextafter(half, 65536.0f)}) {
                dac.set_dac_value_volts(0, value / 65536.0f);
                check(code(0) == static_cast<uint32_t>(std::round(value)), "half-code boundary changed");
            }
        }
    } else if (test == "overflow") {
        auto coefficients = stored;
        coefficients[0] = std::numeric_limits<float>::max();
        check(dac.set_calibration_coeffs(coefficients) == sizeof(coefficients), "finite coefficients rejected");
        dac.set_dac_value_volts(0, 2.5);
        no_data_writes();
        check(dac.get_dac_values()[0] == 0, "overflow published voltage");
    } else if (test == "save_failure") {
        auto coefficients = stored; coefficients[0] = 10000;
        fail_write = true;
        check(dac.set_calibration_coeffs(coefficients) == -1, "failed save accepted");
        fail_write = false;
        dac.set_dac_value_volts(0, 1.0);
        check(code(0) == 20000, "failed save changed active calibration");
    } else if (test == "bad_calibration") {
        for (unsigned i = 0; i < stored.size(); ++i) {
            auto coefficients = stored; coefficients[i] = quiet_nan;
            check(dac.set_calibration_coeffs(coefficients) == -1, "NaN calibration accepted");
            coefficients[i] = std::numeric_limits<float>::infinity();
            check(dac.set_calibration_coeffs(coefficients) == -1, "infinite calibration accepted");
        }
        check(eeprom_writes == 0, "invalid calibration reached EEPROM");
        stored.fill(quiet_nan);
        dac.init();
        dac.set_dac_value_volts(0, 1.0);
        check(code(0) == 20000, "invalid load replaced known calibration");
    } else if (test == "reload_failure") {
        dac.set_dac_value_volts(0, 1.0);
        stored.fill(1000); fail_read = true;
        dac.init();
        for (auto value : dac.get_dac_values()) { check(value == 0, "reset left stale voltages"); }
        dac.set_dac_value_volts(0, 1.0);
        check(code(0) == 20000, "partial load replaced known calibration");
    } else { check(false, "unknown case"); }
    return 0;
}
