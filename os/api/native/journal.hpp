#pragma once
#include "common.hpp"
#include <functional>

namespace koheron::management {
struct LogEntry {
    std::string cursor;
    std::string invocation;
    std::string message;
    std::optional<std::uint64_t> timestamp;
    int priority = 6;
    bool truncated = false;
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
// Construct and use the journal reader on the event thread. Empty output means
// no change; an empty log batch every two seconds keeps the browser watchdog alive.
[[nodiscard]] bool valid_invocation(std::string_view value);
[[nodiscard]] std::function<std::string()> follow_logs(std::string unit, std::optional<std::string> cursor,
    std::optional<std::string> invocation = {}, fs::path directory = {});
} // namespace koheron::management
