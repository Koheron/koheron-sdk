#include "journal.hpp"

#include <systemd/sd-journal.h>
#include <systemd/sd-id128.h>
#include <algorithm>
#include <charconv>
#include <utility>

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
std::string value(sd_journal* reader, const char* name, std::size_t limit = 64 * 1024) {
    const void* data = nullptr;
    std::size_t size = 0;
    if (sd_journal_get_data(reader, name, &data, &size) < 0) return {};
    const std::string_view field(static_cast<const char*>(data), size);
    const auto equals = field.find('=');
    return equals == field.npos ? "" : std::string(field.substr(equals + 1, limit));
}
LogEntry entry(sd_journal* reader, std::size_t message_limit = 64 * 1024) {
    LogEntry result;
    std::uint64_t timestamp = 0;
    if (sd_journal_get_realtime_usec(reader, &timestamp) == 0) result.timestamp = timestamp;
    char* cursor = nullptr;
    if (sd_journal_get_cursor(reader, &cursor) == 0) {
        std::unique_ptr<char, decltype(&std::free)> owned(cursor, std::free);
        result.cursor = cursor;
    }
    result.invocation = value(reader, "_SYSTEMD_INVOCATION_ID");
    result.message = value(reader, "MESSAGE", message_limit);
    const auto priority = value(reader, "PRIORITY");
    int parsed = 6;
    const auto [end, error] = std::from_chars(priority.data(), priority.data() + priority.size(), parsed);
    if (error == std::errc{} && end == priority.data() + priority.size() && parsed >= 0 && parsed <= 7) result.priority = parsed;
    return result;
}
Json log_json(const LogEntry& entry) {
    auto item = object();
    add(item, "ts", entry.timestamp ? integer(*entry.timestamp) : Json{});
    add(item, "msg", text(entry.message));
    add(item, "prio", integer(entry.priority));
    return item;
}

class Follower {
    Reader reader_{nullptr, sd_journal_close};
    std::string unit_;
    std::optional<std::string> cursor_;
    std::optional<LogEntry> pending_;
    bool initialized_ = false, reset_ = false;
    std::chrono::steady_clock::time_point next_heartbeat_{};
    void tail() {
        if (sd_journal_seek_tail(reader_.get()) < 0) throw std::runtime_error("Cannot seek journal");
        const int advanced = sd_journal_previous_skip(reader_.get(), 200);
        if (advanced < 0) throw std::runtime_error("Cannot read journal");
        if (advanced > 0) pending_ = entry(reader_.get(), 4096);
    }
public:
    Follower(std::string unit, std::optional<std::string> cursor) : unit_(std::move(unit)), cursor_(std::move(cursor)) {}
    std::string next() {
        if (!initialized_) {
            reader_ = open(unit_, {});
            if (!reader_) throw std::runtime_error("Cannot open journal");
            sd_journal_set_data_threshold(reader_.get(), 4096);
            if (cursor_) {
                if (sd_journal_seek_cursor(reader_.get(), cursor_->c_str()) < 0 ||
                    sd_journal_next(reader_.get()) <= 0 || sd_journal_test_cursor(reader_.get(), cursor_->c_str()) <= 0) {
                    reset_ = true; cursor_.reset(); tail();
                }
            } else tail();
            initialized_ = true;
        }
        // Zero timeout: process journal notifications without blocking other peers.
        if (sd_journal_wait(reader_.get(), 0) < 0) throw std::runtime_error("Journal watch failed");
        auto result = object(), values = array();
        std::size_t bytes = 0;
        for (unsigned count = 0; count < 200; ++count) {
            if (!pending_) {
                const int advanced = sd_journal_next(reader_.get());
                if (advanced < 0) throw std::runtime_error("Journal read failed");
                if (advanced == 0) break;
                pending_ = entry(reader_.get(), 4096);
                pending_->message = utf8(pending_->message);
            }
            auto item = log_json(*pending_);
            const auto size = encode(item).size();
            // Count encoded bytes, including JSON escaping; retain the next entry.
            if (bytes + size > 60 * 1024) break;
            bytes += size; cursor_ = pending_->cursor;
            append(values, std::move(item)); pending_.reset();
        }
        const auto now = std::chrono::steady_clock::now();
        if (!bytes && !reset_ && now < next_heartbeat_) return {};
        next_heartbeat_ = now + std::chrono::seconds(2);
        add(result, "type", text("logs"));
        add(result, "cursor", cursor_ ? text(*cursor_) : Json{});
        add(result, "entries", std::move(values));
        add(result, "reset", boolean(std::exchange(reset_, false)));
        return encode(result);
    }
};
}

Json LogResult::json() const {
    auto result = object(), values = array();
    for (const auto& entry : entries) {
        append(values, log_json(entry));
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
std::function<std::string()> follow_logs(std::string unit, std::optional<std::string> cursor) {
    const auto follower = std::make_shared<Follower>(std::move(unit), std::move(cursor));
    return [follower] { return follower->next(); };
}
} // namespace koheron::management
