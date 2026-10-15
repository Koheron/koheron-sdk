#ifndef KOHERON_BIST_UPLOAD_HPP
#define KOHERON_BIST_UPLOAD_HPP
#include "measurements.hpp"
#include <arpa/inet.h>
#include <span>

namespace bist
{
constexpr uint32_t upload_bytes = 128 * 1024;
constexpr unsigned upload_requests_per_second = 100;
constexpr std::array<uint32_t, 2> upload_seeds{0x13579bdf, 0x2468ace0};

inline uint32_t upload_ack(std::span<const uint32_t> words, uint32_t seed)
{
    if (words.size() != upload_bytes / sizeof(uint32_t))
        return 0;
    for (size_t i = 0; i < words.size(); ++i)
        if (words[i] != pattern(uint32_t(i), seed))
            return 0;
    return seed ^ 0x42495354U ^ (upload_bytes / sizeof(uint32_t));
}

inline std::vector<uint32_t> upload_request(uint32_t operation, uint32_t seed)
{
    constexpr size_t words = upload_bytes / sizeof(uint32_t);
    std::vector<uint32_t> request(words + 4);
    request[0] = 0;
    request[1] = htonl(operation);
    request[2] = htonl(upload_bytes);
    for (size_t i = 0; i < words; ++i)
        request[i + 3] = pattern(uint32_t(i), seed);
    request.back() = htonl(seed);
    return request;
}
} // namespace bist
#endif
