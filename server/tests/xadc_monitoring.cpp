#include "boards/alpha250/drivers/temperature-sensor.hpp"
#include "server/drivers/xadc.hpp"
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <new>
#include <numeric>
#include <string_view>
#include <vector>

namespace {
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
// UG480 temperature/supply equations use a 12-bit ADC code. The status register
// carries that code in its high 12 bits; retain the four fractional bits.
// https://docs.amd.com/r/en-US/ug480_7Series_XADC/ADC-Transfer-Functions
float temperature(long double register_value) {
    return static_cast<float>((register_value / 16) * 503.975L / 4096 - 273.15L);
}
void set_code(uint16_t code) { hw::xadc_fixture.registers[0x200 / 4] = code; }
void close(float value, float expected, const char* message) {
    check(std::isfinite(value) && std::abs(value - expected) <= 0.00004f, message);
}
}
int main(int argc, char** argv) {
    check(argc == 2, "case");
    const std::string_view test{argv[1]};
    if (test == "startup") {
        // Default construction must not depend on previously occupied storage.
        alignas(TemperatureSensor) std::byte storage[sizeof(TemperatureSensor)];
        std::memset(storage, 0xff, sizeof(storage));
        auto* sensor = ::new (storage) TemperatureSensor;
        set_code(40000);
        close(sensor->get_zynq_temperature(), temperature(40000), "default-initialized first sample");
        sensor->~TemperatureSensor();
        TemperatureSensor zero_initialized{};
        close(zero_initialized.get_zynq_temperature(), temperature(40000), "value-initialized first sample");
    } else if (test == "warmup") {
        TemperatureSensor sensor{};
        std::vector<uint16_t> seen;
        for (unsigned i = 0; i < 100; ++i) {
            const auto code = static_cast<uint16_t>(30000 + i * 101);
            seen.push_back(code);
            set_code(code);
            const auto sum = std::accumulate(seen.begin(), seen.end(), uint64_t{});
            close(sensor.get_zynq_temperature(), temperature(static_cast<long double>(sum) / seen.size()),
                  "warmup must average only acquired samples");
        }
    } else if (test == "window") {
        TemperatureSensor sensor{};
        set_code(30000);
        for (unsigned i = 0; i < 100; ++i) { sensor.get_zynq_temperature(); }
        set_code(50000);
        for (unsigned i = 1; i <= 100; ++i) {
            close(sensor.get_zynq_temperature(), temperature(((100 - i) * 30000 + i * 50000) / 100.L),
                  "oldest sample was not replaced");
        }
        set_code(0);
        for (unsigned i = 1; i <= 100; ++i) {
            close(sensor.get_zynq_temperature(), temperature((100 - i) * 50000 / 100.L),
                  "ring wrap or zero-code conversion");
        }
    } else if (test == "drift") {
        TemperatureSensor sensor{};
        std::deque<uint16_t> history;
        uint32_t state = 17;
        for (unsigned i = 0; i < 200000; ++i) {
            state = state * 1664525U + 1013904223U;
            const auto code = static_cast<uint16_t>(state >> 16);
            set_code(code);
            history.push_back(code);
            if (history.size() > 100) { history.pop_front(); }
            const float value = sensor.get_zynq_temperature();
            if (i % 97 == 0) {
                const auto sum = std::accumulate(history.begin(), history.end(), uint64_t{});
                close(value, temperature(static_cast<long double>(sum) / history.size()), "long-run average drift");
            }
        }
        set_code(65535);
        for (unsigned i = 0; i < 100; ++i) { sensor.get_zynq_temperature(); }
        close(sensor.get_zynq_temperature(), temperature(65535), "full-scale sum overflow");
    } else if (test == "instances") {
        TemperatureSensor first{}, second{};
        for (unsigned i = 0; i < 300; ++i) {
            set_code(12345);
            close(first.get_zynq_temperature(), temperature(12345), "first instance history");
            set_code(54321);
            close(second.get_zynq_temperature(), temperature(54321), "second instance history");
        }
    } else if (test == "xadc") {
        Xadc xadc;
        const std::array<uint32_t, 6> offsets{0x204, 0x208, 0x218, 0x234, 0x238, 0x23c};
        const std::array getters{&Xadc::get_PlVccInt, &Xadc::get_PlVccAux, &Xadc::get_PlVccBram,
                                 &Xadc::get_PsVccInt, &Xadc::get_PsVccAux, &Xadc::get_PsVccMem};
        for (const auto code : {0, 1, 16, 32768, 65535}) {
            set_code(static_cast<uint16_t>(code));
            close(xadc.get_temperature(), temperature(code), "XADC temperature scaling");
            for (size_t i = 0; i < offsets.size(); ++i) {
                const auto sample = static_cast<uint16_t>(code + i * 5000);
                hw::xadc_fixture.registers[offsets[i] / 4] = sample;
                const auto expected = static_cast<float>(static_cast<long double>(sample) / 16 * 3 / 4096);
                check(std::abs((xadc.*getters[i])() - expected) <= 0.0000002f, "XADC rail scaling/register");
            }
        }
    } else { check(false, "unknown case"); }
}
