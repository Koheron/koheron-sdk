#ifndef EEPROM_HEADER
#define EEPROM_HEADER "boards/alpha250/drivers/eeprom.hpp"
#endif
#include EEPROM_HEADER
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
std::array<uint8_t, 8192> storage{};
struct Packet { unsigned address, size; };
std::vector<Packet> packets;
unsigned cursor = 0, writes = 0, reads = 0, busy = 0;
bool fail_write = false, fail_read = false, program_busy = false;
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
void reset() {
    storage.fill(0xA5);
    packets.clear();
    cursor = writes = reads = busy = 0;
    fail_write = fail_read = program_busy = false;
}
template<unsigned Offset, class T, size_t N>
void round_trip(Eeprom& eeprom, const std::array<T, N>& data) {
    constexpr auto bytes = sizeof(T) * N;
    check(eeprom.write<Offset>(data) == static_cast<int>(bytes), "write failed");
    const auto* raw = reinterpret_cast<const uint8_t*>(data.data());
    for (unsigned i = 0; i < storage.size(); ++i) {
        const auto expected = i >= Offset && i < Offset + bytes ? raw[i - Offset] : uint8_t{0xA5};
        check(storage[i] == expected, "page wrap corrupted payload or neighboring bytes");
    }
    unsigned address = Offset, remaining = bytes;
    for (const auto packet : packets) {
        const auto size = std::min(32u - address % 32u, remaining);
        check(packet.address == address && packet.size == size, "incorrect page packet");
        remaining -= size;
        address += size;
    }
    check(remaining == 0, "missing page packet");
    std::array<T, N> received{};
    check(eeprom.read<Offset>(received) == static_cast<int>(bytes), "read failed");
    check(std::memcmp(received.data(), data.data(), bytes) == 0, "readback mismatch");
}
}

// Model 24LC64 page wrapping instead of rejecting an invalid packet outright.
namespace hw {
I2cDev::I2cDev(std::string name) : devname(std::move(name)) {}
I2cDev::~I2cDev() = default;
I2cManager::I2cManager() : empty_i2cdev(std::make_unique<I2cDev>("fixture")) {}
I2cDev& I2cManager::get(const std::string& name) {
    check(name == "i2c-0", "bus changed"); return *empty_i2cdev;
}
int I2cDev::write(int32_t addr, const uint8_t* buffer, size_t size) {
    check(addr == 0x54 && size >= 2 && size <= 34, "invalid EEPROM transaction");
    ++writes;
    if (fail_write || busy != 0) {
        if (busy) { --busy; }
        errno = ENXIO;
        return -1;
    }
    cursor = (unsigned(buffer[0]) << 8) | buffer[1];
    check(cursor < storage.size(), "invalid memory address");
    if (size > 2) {
        packets.push_back({cursor, static_cast<unsigned>(size - 2)});
        const auto page = cursor / 32 * 32;
        for (size_t i = 2; i < size; ++i) { storage[page + (cursor + i - 2) % 32] = buffer[i]; }
        if (program_busy) { busy = 1; }
    }
    return static_cast<int>(size);
}
int I2cDev::read(int32_t addr, uint8_t* buffer, size_t size) {
    check(addr == 0x54, "read address changed");
    ++reads;
    if (fail_read) {
        std::memset(buffer, 0xCC, size); // A failed read can leave partial data.
        return -1;
    }
    for (size_t i = 0; i < size; ++i) { buffer[i] = storage[(cursor + i) % storage.size()]; }
    cursor = (cursor + size) % storage.size();
    return static_cast<int>(size);
}
}

int main(int argc, char** argv) {
    check(argc == 2, "case required");
    auto service = services::provide<hw::I2cManager>();
    Eeprom eeprom;
    const std::string_view test{argv[1]};
    reset();
    if (test == "offsets") {
        std::array<uint8_t, 65> data{};
        for (unsigned i = 0; i < data.size(); ++i) { data[i] = static_cast<uint8_t>(i + 1); }
        [&]<size_t... I>(std::index_sequence<I...>) {
            ((reset(), round_trip<256 + I>(eeprom, data)), ...);
        }(std::make_index_sequence<32>{});
    } else if (test == "aligned") {
        std::array<uint32_t, 16> data{};
        for (unsigned i = 0; i < data.size(); ++i) { data[i] = 0x12345600 + i; }
        round_trip<256>(eeprom, data);
        check(packets.size() == 2 && writes == 3 && reads == 1, "aligned transaction count changed");
    } else if (test == "typed") {
        const std::array<uint32_t, 3> data{0x12345678, 0xAABBCCDD, 0x01020304};
        round_trip<31>(eeprom, data);
    } else if (test == "end") {
        round_trip<8191>(eeprom, std::array<uint8_t, 1>{42});
        reset();
        round_trip<8160>(eeprom, std::array<uint8_t, 32>{42});
    } else if (test == "empty") {
        std::array<uint8_t, 0> data{};
        check(eeprom.write<8192>(data) == 0 && eeprom.read<8192>(data) == 0, "empty result");
        check(writes == 0 && reads == 0, "empty operation touched I2C");
    } else if (test == "serial") {
        check(eeprom.set_serial_number(0x12345678) == 4 && eeprom.get_serial_number() == 0x12345678,
              "serial round trip");
        fail_read = true;
        check(eeprom.get_serial_number() == 0, "failed serial read exposed partial data");
        fail_read = false; fail_write = true;
        check(eeprom.get_serial_number() == 0, "failed serial selection exposed data");
    } else if (test == "busy") {
        program_busy = true;
        round_trip<31>(eeprom, std::array<uint8_t, 34>{42});
        check(packets.size() == 3 && writes == 7 && reads == 1, "busy retries changed");
    } else { check(false, "unknown case"); }
    services::remove<hw::I2cManager>();
}
