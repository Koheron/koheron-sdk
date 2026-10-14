#ifndef KOHERON_BIST_METADATA_HPP
#define KOHERON_BIST_METADATA_HPP
#include "measurements.hpp"
#include <map>

namespace bist
{
using Fields = std::map<std::string, std::string>;

// Read release/manifest assignments as data. Never source them in a shell.
inline Fields release_fields(std::string_view text)
{
    Fields fields;
    std::istringstream input{std::string(text)};
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const auto equal = line.find('=');
        if (equal == std::string::npos || equal == 0 || line.front() == '#')
            continue;
        const auto key = line.substr(0, equal);
        auto value = line.substr(equal + 1);
        if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') &&
            value.back() == value.front())
        {
            const char delimiter = value.front();
            value = value.substr(1, value.size() - 2);
            if (delimiter == '"')
            {
                std::string decoded;
                for (size_t i = 0; i < value.size(); ++i)
                {
                    if (value[i] == '\\' && i + 1 < value.size() &&
                        std::string_view("\"\\$`").find(value[i + 1]) != std::string_view::npos)
                        ++i;
                    decoded += value[i];
                }
                value = std::move(decoded);
            }
        }
        fields[key] = std::move(value);
    }
    return fields;
}

inline std::string fields_json(const Fields& fields)
{
    std::string result = "{";
    for (const auto& [key, value] : fields)
    {
        if (result.size() > 1)
            result += ',';
        result += quote(key) + ':' + quote(value);
    }
    return result + '}';
}

inline std::string field(const Fields& fields, const std::string& key)
{
    const auto it = fields.find(key);
    return it == fields.end() || it->second.empty() ? "unavailable" : it->second;
}
} // namespace bist
#endif
