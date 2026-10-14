#include "../upload.hpp"
#include "../pacing.hpp"
#include <cassert>

int main()
{
    using namespace bist;
    for (const auto seed : upload_seeds)
    {
        auto packet = upload_request(0x12340056, seed);
        assert(packet.size() * 4 == upload_bytes + 16);
        assert(packet[0] == 0 && ntohl(packet[1]) == 0x12340056);
        assert(ntohl(packet[2]) == upload_bytes && ntohl(packet.back()) == seed);
        auto words = std::span{packet}.subspan(3, upload_bytes / 4);
        const auto ack = upload_ack(words, seed);
        assert(ack != 0);
        assert(upload_ack(words, seed ^ 1) == 0);
        assert(upload_ack(words.first(words.size() - 1), seed) == 0);
        assert(upload_ack({}, seed) == 0);
        for (const size_t index : {size_t{0}, words.size() / 2, words.size() - 1})
        {
            words[index] ^= 1;
            assert(upload_ack(words, seed) == 0);
            words[index] ^= 1;
            assert(upload_ack(words, seed) == ack);
        }
    }
    assert(comparison_parameters(15).find("\"upload\"") == std::string::npos);
    assert(comparison_parameters(48).find("\"upload\"") != std::string::npos);
    assert(comparison_cases[4] == "upload_value" && comparison_cases[5] == "upload_reference");
}
