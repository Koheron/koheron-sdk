#include "boards/alpha250/drivers/power-monitor.hpp"
#include "server/hardware/i2c_manager.hpp"
#include "server/runtime/services.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
volatile uint16_t registers[2][3]{};
uint8_t selected[2]{};
unsigned writes = 0, reads = 0, configurations = 0;
bool fail_write = false, fail_read = false;
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
void close(float actual, double expected) {
    check(std::abs(static_cast<double>(actual) - expected) <= std::abs(expected) * 2e-7 + 1e-12,
          "incorrect voltage/current conversion");
}
unsigned supply(int32_t addr) {
    check(addr == 0x41 || addr == 0x45, "invalid I2C address");
    return addr == 0x41 ? 0 : 1;
}
}

// Simulate only the I2C boundary, including big-endian wire bytes.
namespace hw {
I2cDev::I2cDev(std::string name) : devname(std::move(name)) {}
I2cDev::~I2cDev() = default;
I2cManager::I2cManager() : empty_i2cdev(std::make_unique<I2cDev>("fixture")) {}
I2cDev& I2cManager::get(const std::string& name) {
    check(name == "i2c-0", "I2C bus changed");
    return *empty_i2cdev;
}
int I2cDev::write(int32_t addr, const uint8_t* buffer, size_t size) {
    const auto index = supply(addr);
    ++writes;
    if (fail_write) { return -1; }
    if (size == 3) {
        check(buffer[0] == 0 && buffer[1] == 0x43 && buffer[2] == 0xFF, "configuration changed");
        ++configurations;
    } else {
        check(size == 1 && (buffer[0] == 1 || buffer[0] == 2), "register selection changed");
        selected[index] = buffer[0];
    }
    return static_cast<int>(size);
}
int I2cDev::read(int32_t addr, uint8_t* buffer, size_t size) {
    const auto index = supply(addr);
    ++reads;
    check(size == 2, "read size changed");
    buffer[0] = 0xFF; // Failed reads may have modified part of the destination.
    if (fail_read) { return -1; }
    const auto code = registers[index][selected[index]];
    buffer[0] = static_cast<uint8_t>(code >> 8);
    buffer[1] = static_cast<uint8_t>(code);
    return 2;
}
}

int main(int argc, char** argv) {
    check(argc == 2, "case required");
    const std::string_view test{argv[1]};
    auto bus = services::provide<hw::I2cManager>();
    PowerMonitor monitor;
    check(configurations == 2 && writes == 2 && reads == 0, "both supplies must be configured");
    writes = 0;
    if (test == "bounds") {
        for (const auto index : {2u, 3u, 255u, std::numeric_limits<uint32_t>::max()}) {
            check(monitor.get_shunt_voltage(index) == -1.0f, "invalid shunt index accepted");
            check(monitor.get_bus_voltage(index) == -1.0f, "invalid bus index accepted");
        }
        check(writes == 0 && reads == 0, "invalid index reached I2C");
    } else if (test == "shunt") {
        for (unsigned index = 0; index < 2; ++index) {
            // Start with negative one to make the signedness regression explicit.
            registers[index][1] = 0xFFFF;
            close(monitor.get_shunt_voltage(index), -2.5e-6);
            for (uint32_t raw = 0; raw <= 0xFFFF; ++raw) {
                registers[index][1] = static_cast<uint16_t>(raw);
                const auto signed_code = static_cast<int32_t>(raw) - (raw >= 0x8000 ? 65536 : 0);
                close(monitor.get_shunt_voltage(index), signed_code * 2.5e-6);
            }
        }
        check(reads == 2 * 65537 && writes == reads, "shunt transaction count changed");
    } else if (test == "bus") {
        for (unsigned index = 0; index < 2; ++index) {
            for (uint32_t raw = 0; raw <= 0x7FFF; ++raw) {
                registers[index][2] = static_cast<uint16_t>(raw);
                close(monitor.get_bus_voltage(index), raw / 800.0);
            }
        }
        check(reads == 2 * 32768 && writes == reads, "bus transaction count changed");
    } else if (test == "ui") {
        registers[0][1] = 4000; registers[0][2] = 9600;
        registers[1][1] = 0xFFFC; registers[1][2] = 2640;
        const auto values = monitor.get_supplies_ui();
        const std::array<double, 4> expected{1.0, 12.0, -0.001, 3.3};
        for (size_t i = 0; i < values.size(); ++i) { close(values[i], expected[i]); }
        check(reads == 4 && writes == 4, "UI transaction count changed");
    } else if (test == "write_failure" || test == "read_failure") {
        fail_write = test == "write_failure";
        fail_read = !fail_write;
        for (unsigned index = 0; index < 2; ++index) {
            check(monitor.get_shunt_voltage(index) == -1.0f, "shunt error lost");
            check(monitor.get_bus_voltage(index) == -1.0f, "bus error lost");
        }
        check(writes == 4 && reads == (fail_write ? 0u : 4u), "failed transaction retried/continued");
        fail_write = fail_read = false;
        registers[0][1] = 8000; registers[1][2] = 9584;
        close(monitor.get_shunt_voltage(0), 0.020);
        close(monitor.get_bus_voltage(1), 11.98);
    } else if (test == "benchmark") {
        registers[0][1] = 4000; registers[0][2] = 9600;
        registers[1][1] = 2000; registers[1][2] = 2640;
        constexpr unsigned iterations = 1000000;
        std::array<double, 11> times{};
        float sum = 0;
        for (auto& time : times) {
            const auto start = std::chrono::steady_clock::now();
            for (unsigned i = 0; i < iterations; ++i) {
                const auto values = monitor.get_supplies_ui();
                sum += values[i % values.size()];
            }
            time = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / iterations;
        }
        std::sort(times.begin(), times.end());
        std::cout << "median_ns " << times[5] << " checksum " << sum << '\n';
    } else { check(false, "unknown case"); }
    services::remove<hw::I2cManager>();
}
