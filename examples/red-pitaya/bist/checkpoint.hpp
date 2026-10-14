#ifndef KOHERON_BIST_CHECKPOINT_HPP
#define KOHERON_BIST_CHECKPOINT_HPP
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
namespace bist
{
// One bounded, checksummed generation. JSON/text exports are derived files.
using Checkpoint = std::array<std::string, 9>;
inline uint32_t crc32(std::string_view bytes)
{
    uint32_t crc = ~0U;
    for (unsigned char byte : bytes)
    {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
inline void append_word(std::string& out, uint32_t word)
{
    for (unsigned i = 0; i < 4; ++i)
        out += char(word >> (8 * i));
}
inline std::string encode_checkpoint(const Checkpoint& fields)
{
    std::string out = "BISTCP01";
    for (const auto& field : fields)
    {
        append_word(out, field.size());
        out += field;
    }
    append_word(out, crc32(out));
    return out;
}
inline bool decode_checkpoint(std::string_view bytes, Checkpoint& fields)
{
    if (bytes.size() < 48 || bytes.size() > 8 * 1024 * 1024 || bytes.substr(0, 8) != "BISTCP01")
        return false;
    auto word = [&](size_t offset)
    {
        uint32_t result = 0;
        for (unsigned i = 0; i < 4; ++i)
            result |= uint32_t(static_cast<unsigned char>(bytes[offset + i])) << (8 * i);
        return result;
    };
    const size_t end = bytes.size() - 4;
    if (word(end) != crc32(bytes.substr(0, end)))
        return false;
    size_t offset = 8;
    Checkpoint decoded;
    for (auto& field : decoded)
    {
        if (end - offset < 4)
            return false;
        const size_t length = word(offset);
        offset += 4;
        if (length > end - offset)
            return false;
        field = bytes.substr(offset, length);
        offset += length;
    }
    if (offset != end)
        return false;
    fields = std::move(decoded);
    return true;
}
} // namespace bist
#endif
