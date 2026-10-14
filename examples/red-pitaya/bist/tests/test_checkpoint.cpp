#include "../checkpoint.hpp"
#include "../workload.hpp"
#include <cassert>
#include <thread>
int main()
{
    bist::Checkpoint original{"[]", "{}", "versions\n", "running", std::string("log\0tail", 8), "scorecard",
                              "pass", "not_evaluated", ""}, decoded;
    const auto bytes = bist::encode_checkpoint(original);
    assert(bist::decode_checkpoint(bytes, decoded) && decoded == original);
    for (size_t length = 0; length < bytes.size(); ++length)
        assert(!bist::decode_checkpoint(bytes.substr(0, length), decoded));
    for (size_t offset = 0; offset < bytes.size(); ++offset)
    {
        auto damaged = bytes;
        damaged[offset] ^= 1;
        assert(!bist::decode_checkpoint(damaged, decoded));
    }
    assert(!bist::decode_checkpoint(bytes + "junk", decoded));
    auto oversized = bytes;
    for (size_t i = 8; i < 12; ++i) oversized[i] = char(255);
    oversized.resize(oversized.size() - 4);
    bist::append_word(oversized, bist::crc32(oversized));
    assert(!bist::decode_checkpoint(oversized, decoded));
    bist::StartGate gate(3);
    std::thread a([&] { gate.wait(); assert(gate.start() != bist::Clock::time_point{}); });
    std::thread b([&] { gate.drop(); });
    gate.wait(); a.join(); b.join();
    assert(gate.failures() == 1);
}
