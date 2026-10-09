#include "boards/alpha250/drivers/precision-adc.hpp"
#include "server/hardware/spi_manager.hpp"
#include "server/runtime/services.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
using namespace std::chrono_literals;

namespace {
bool available = true;
std::atomic<bool> block_read{false}, entered{false}, fail_read{false};
std::atomic<unsigned> reads{0};
std::atomic<unsigned> active_readers{0};
struct ReaderLifetime {
    ReaderLifetime() { ++active_readers; }
    ~ReaderLifetime() { --active_readers; }
};
unsigned setup_writes = 0;
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
template<class F> void wait_until(F condition, const char* message = "condition timed out") {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!condition()) {
        check(std::chrono::steady_clock::now() < deadline, message);
        std::this_thread::sleep_for(1ms);
    }
}
void check_stopped() {
    check(active_readers == 0, "worker did not exit before destruction returned");
    const auto count = reads.load();
    std::this_thread::sleep_for(25ms);
    check(reads == count, "SPI read after driver destruction");
}
}

// Fake only the hardware boundary; compile the production ADC implementation.
namespace hw {
SpiDev::~SpiDev() = default;
int SpiDev::init(uint8_t, uint32_t, uint8_t) { fd = 123; return 0; }
int SpiDev::set_mode(uint8_t mode_) { check(mode_ == SPI_MODE_3, "SPI mode changed"); return 0; }
int SpiDev::set_speed(uint32_t speed_) { check(speed_ == 10000000, "SPI speed changed"); return 0; }
SpiManager::SpiManager() { if (available) { empty_spidev.init(0, 0, 0); } }
SpiDev& SpiManager::get(std::string_view name, uint8_t, uint32_t, uint8_t) {
    check(name == "spidev1.0", "SPI device changed");
    return empty_spidev;
}
int SpiDev::transfer(std::span<const uint8_t> tx, std::span<uint8_t> rx) {
    check(tx.size() == rx.size(), "SPI sizes");
    if (tx[0] == 0x45) { rx[1] = 0xA5; return 2; }
    if (tx[0] != 0x42) { ++setup_writes; return static_cast<int>(tx.size()); }
    check(tx.size() == 5, "ADC frame size changed");
    // Its destructor runs on worker exit, before an owner's join returns.
    static thread_local ReaderLifetime lifetime;
    entered = true;
    block_read.wait(true);
    const auto sample = reads.fetch_add(1);
    if (fail_read) { return -1; }
    const auto channel = sample % 8;
    rx[1] = static_cast<uint8_t>(0x80 + channel);
    rx[2] = rx[3] = 0;
    rx[4] = static_cast<uint8_t>(channel);
    return 5;
}
}

int main(int argc, char** argv) {
    check(argc == 2, "test case required");
    const std::string_view test{argv[1]};
    available = test != "unavailable";
    auto spi = services::provide<hw::SpiManager>();
    if (test == "unavailable") {
        PrecisionAdc adc;
        for (auto value : adc.get_adc_values()) { check(value == 0.0f, "initial data"); }
        check(active_readers == 0 && reads == 0 && setup_writes == 0, "worker without SPI");
    } else if (test == "samples") {
        {
            PrecisionAdc adc;
            check(setup_writes == 11, "ADC setup changed");
            check(adc.get_device_id() == 0xA5, "device ID read");
            wait_until([&] { return adc.get_adc_values()[7] != 0.0f; });
            const auto values = adc.get_adc_values();
            for (unsigned i = 0; i < values.size(); ++i) {
                check(values[i] == static_cast<float>(i) * 1.25f / 128, "sample conversion changed");
            }
        }
        check_stopped();
    } else if (test == "inflight") {
        block_read = true;
        auto adc = std::make_unique<PrecisionAdc>();
        wait_until([] { return entered.load(); });
        std::atomic<bool> deleting{false}, deleted{false};
        std::thread destroy([&] { deleting = true; adc.reset(); deleted = true; });
        wait_until([&] { return deleting.load(); });
        std::this_thread::sleep_for(25ms);
        check(!deleted, "destructor did not join in-flight SPI read");
        block_read = false;
        block_read.notify_all();
        destroy.join();
        check(reads == 1, "worker started another transfer during shutdown");
        check_stopped();
    } else if (test == "errors") {
        fail_read = true;
        {
            PrecisionAdc adc;
            wait_until([] { return reads >= 2; });
        }
        check_stopped();
    } else if (test == "immediate") {
        for (unsigned i = 0; i < 50; ++i) { PrecisionAdc adc; }
        check_stopped();
    } else if (test == "readers") {
        {
            PrecisionAdc adc;
            std::atomic<bool> done{false};
            std::thread reader([&] {
                while (!done) {
                    auto values = adc.get_adc_values();
                    for (unsigned i = 0; i < values.size(); ++i) {
                        check(values[i] == 0.0f || values[i] == static_cast<float>(i) * 1.25f / 128,
                              "torn sample snapshot");
                    }
                }
            });
            wait_until([] { return reads >= 12; });
            done = true;
            reader.join();
        }
        check_stopped();
    } else if (test == "cadence") {
        // Benchmark mode exits without destroying the old detached-thread driver.
        PrecisionAdc adc;
        wait_until([] { return reads >= 2; });
        const auto count = reads.load();
        const auto start = std::chrono::steady_clock::now();
        while (reads < count + 200) { std::this_thread::sleep_for(1ms); }
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "seconds_per_200_samples " << seconds << std::endl;
        std::_Exit(0);
    } else { check(false, "unknown case"); }
    check(active_readers == 0, "worker thread leaked");
    services::remove<hw::SpiManager>();
}
