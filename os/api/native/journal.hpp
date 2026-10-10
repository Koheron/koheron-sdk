#pragma once
#include "common.hpp"

namespace koheron::management {
struct LogEntry {
    std::string cursor;
    std::string invocation;
    std::string message;
    std::optional<std::uint64_t> timestamp;
};
struct LogResult {
    std::vector<LogEntry> entries;
    std::optional<std::string> cursor;
    [[nodiscard]] Json json() const;
};
[[nodiscard]] LogResult read_logs(std::string_view unit, const std::optional<std::string>& cursor,
    int limit, const std::optional<std::string>& invocation = {},
    const std::optional<std::uint64_t>& min_timestamp = {});
[[nodiscard]] std::optional<LogEntry> bookmark(std::string_view unit,
    const std::optional<std::string>& invocation = {});
} // namespace koheron::management
