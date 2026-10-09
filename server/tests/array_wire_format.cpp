// Run: g++ -std=c++23 -I. -Iserver/external_libs -Iserver/external_libs/eigen server/tests/array_wire_format.cpp -o /tmp/array-wire-test && /tmp/array-wire-test
#include "server/network/serializer_deserializer.hpp"
#include <cassert>

int main() {
    // An ordinary big-endian scalar followed by raw little-endian DAC words.
    // Distinct bytes catch both channel swapping and per-channel byte reversal.
    static_assert(std::endian::native == std::endian::little);
    const unsigned char wire[] = {
        0x12, 0x34, 0x56, 0x78,
        0x34, 0x12, 0xcd, 0xab, 0xff, 0x7f, 0x00, 0x80
    };
    auto [scalar, words] = net::deserialize<0, uint32_t, std::array<uint32_t, 2>>(
        reinterpret_cast<const std::byte*>(wire));
    assert(scalar == 0x12345678u);
    assert(words[0] == 0xabcd1234u);
    assert(words[1] == 0x80007fffu);

    const std::array<float, 3> coeffs{1.0f, -2.5f, 0.125f};
    const auto decoded = net::extract<std::array<float, 3>>(
        reinterpret_cast<const std::byte*>(coeffs.data()));
    assert(decoded == coeffs);
    const auto empty = net::extract<std::array<uint32_t, 0>>(nullptr);
    assert(empty.empty());
}
