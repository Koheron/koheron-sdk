#include "journal.hpp"

#include <systemd/sd-journal.h>
#include <systemd/sd-id128.h>
#include <algorithm>

namespace koheron::management {
namespace {
using Reader = std::unique_ptr<sd_journal, decltype(&sd_journal_close)>;
Reader open(std::string_view unit, const std::optional<std::string>& invocation) {
    sd_journal* pointer = nullptr;
    if (sd_journal_open(&pointer, SD_JOURNAL_LOCAL_ONLY) < 0) return {nullptr, sd_journal_close};
    Reader reader(pointer, sd_journal_close);
    sd_id128_t boot;
    if (sd_id128_get_boot(&boot) < 0) return {nullptr, sd_journal_close};
    char id[SD_ID128_STRING_MAX];
    sd_id128_to_string(boot, id);
    const std::string boot_match = std::string("_BOOT_ID=") + id;
    const std::string unit_match = "_SYSTEMD_UNIT=" + std::string(unit);
    if (sd_journal_add_match(reader.get(), boot_match.c_str(), 0) < 0 ||
        sd_journal_add_match(reader.get(), unit_match.c_str(), 0) < 0) return {nullptr, sd_journal_close};
    if (invocation) {
        const std::string match = "_SYSTEMD_INVOCATION_ID=" + *invocation;
        if (sd_journal_add_match(reader.get(), match.c_str(), 0) < 0) return {nullptr, sd_journal_close};
    }
    return reader;
}
std::string value(sd_journal* reader, const char* name) {
    const void* data = nullptr;
    std::size_t size = 0;
    if (sd_journal_get_data(reader, name, &data, &size) < 0) return {};
    const std::string_view field(static_cast<const char*>(data), size);
    const auto equals = field.find('=');
    return equals == field.npos ? "" : std::string(field.substr(equals + 1));
}
LogEntry entry(sd_journal* reader) {
    LogEntry result;
    std::uint64_t timestamp = 0;
    if (sd_journal_get_realtime_usec(reader, &timestamp) == 0) result.timestamp = timestamp;
    char* cursor = nullptr;
    if (sd_journal_get_cursor(reader, &cursor) == 0) {
        std::unique_ptr<char, decltype(&std::free)> owned(cursor, std::free);
        result.cursor = cursor;
    }
    result.invocation = value(reader, "_SYSTEMD_INVOCATION_ID");
    result.message = value(reader, "MESSAGE");
    return result;
}
}

Json LogResult::json() const {
    auto result = object(), values = array();
    for (const auto& entry : entries) {
        auto item = object();
        add(item, "ts", entry.timestamp ? integer(*entry.timestamp) : Json{});
        add(item, "msg", text(entry.message));
        append(values, std::move(item));
    }
    add(result, "cursor", cursor ? text(*cursor) : Json{});
    add(result, "entries", std::move(values));
    return result;
}
LogResult read_logs(std::string_view unit, const std::optional<std::string>& cursor,
    int limit, const std::optional<std::string>& invocation,
    const std::optional<std::uint64_t>& min_timestamp) {
    auto reader = open(unit, invocation);
    LogResult result;
    if (!reader) return result;
    limit = limit <= 0 ? 5000 : std::min(limit, 5000);
    if (cursor) {
        if (sd_journal_seek_cursor(reader.get(), cursor->c_str()) < 0 || sd_journal_next(reader.get()) <= 0) return result;
        // Invocation filtering may place us at the first new startup entry,
        // rather than at the old invocation's bookmark. Do not skip that entry.
        if (sd_journal_test_cursor(reader.get(), cursor->c_str()) == 0) result.entries.push_back(entry(reader.get()));
    } else if (sd_journal_seek_tail(reader.get()) < 0) return result;
    for (int count = static_cast<int>(result.entries.size()); count < limit; ++count) {
        const int advanced = cursor ? sd_journal_next(reader.get()) : sd_journal_previous(reader.get());
        if (advanced <= 0) break;
        result.entries.push_back(entry(reader.get()));
    }
    if (!cursor) std::ranges::reverse(result.entries);
    if (!result.entries.empty() && !result.entries.back().cursor.empty()) result.cursor = result.entries.back().cursor;
    if (min_timestamp) std::erase_if(result.entries, [&](const LogEntry& value) {
        return value.timestamp && *value.timestamp < *min_timestamp;
    });
    if (result.entries.empty() && !result.cursor && invocation)
        return read_logs(unit, cursor, limit, {}, min_timestamp);
    return result;
}
std::optional<LogEntry> bookmark(std::string_view unit, const std::optional<std::string>& invocation) {
    auto reader = open(unit, invocation);
    if (reader && sd_journal_seek_tail(reader.get()) == 0 && sd_journal_previous(reader.get()) > 0) return entry(reader.get());
    if (invocation) return bookmark(unit);
    return {};
}
} // namespace koheron::management
