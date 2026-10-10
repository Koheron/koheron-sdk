#pragma once

#include <json-c/json.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace koheron::management {
namespace fs = std::filesystem;

class Fd {
    int value_ = -1;
public:
    explicit Fd(int value = -1) noexcept : value_(value) {}
    ~Fd();
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept;
    Fd& operator=(Fd&& other) noexcept;
    [[nodiscard]] int get() const noexcept { return value_; }
    [[nodiscard]] int release() noexcept;
};

struct JsonDeleter { void operator()(json_object* value) const noexcept { if (value) json_object_put(value); } };
using Json = std::unique_ptr<json_object, JsonDeleter>;
[[nodiscard]] Json object();
[[nodiscard]] Json array();
[[nodiscard]] Json text(std::string_view value);
[[nodiscard]] Json boolean(bool value);
[[nodiscard]] Json integer(std::uint64_t value);
void add(Json& target, std::string_view key, Json value);
void append(Json& target, Json value);
[[nodiscard]] std::string encode(const Json& value);
[[nodiscard]] Json parse(std::string_view value);
[[nodiscard]] std::string field(json_object* value, const char* key);
[[nodiscard]] std::string trim(std::string_view value);
[[nodiscard]] std::string utf8(std::string_view value);
[[nodiscard]] std::optional<std::string> read_file(const fs::path& path, std::size_t limit = 4 * 1024 * 1024);
void write_all(int fd, std::span<const char> bytes);
void write_file(const fs::path& path, std::string_view bytes, unsigned mode = 0644);
void sync_directory(const fs::path& path);
void atomic_write(const fs::path& path, std::string_view bytes);
[[nodiscard]] Json kv_file(const fs::path& path);
[[nodiscard]] std::uint64_t monotonic_us();
[[nodiscard]] fs::path temporary_directory(const fs::path& parent, std::string_view prefix);
[[nodiscard]] std::pair<fs::path, Fd> temporary_file(const fs::path& parent, std::string_view prefix);
[[nodiscard]] std::string safe_filename(std::string_view filename);
[[nodiscard]] std::uint64_t realtime_us();
[[noreturn]] void system_error(std::string_view context);

struct Settings {
    fs::path instruments = "/usr/local/instruments";
    fs::path live = "/tmp/live-instrument";
    fs::path manifest = "/usr/local/share/koheron/manifest.txt";
    fs::path release = "/etc/koheron-release";
    std::string unit = "koheron-server.service";
    std::string led_unit = "koheron-server-init.service";
    std::string systemctl = "/bin/systemctl";
    // Fixture roots also make collection testable without mocking Linux syscalls.
    fs::path proc = "/proc";
};

// posix_spawn avoids running C++ code in a forked, multithreaded HTTP process.
[[nodiscard]] int command(const std::vector<std::string>& arguments);
void service_action(const Settings& settings, std::string_view action, std::string_view unit);
[[nodiscard]] bool unit_is_active(const Settings& settings);
[[nodiscard]] Json service_status(const Settings& settings, std::string_view unit);
} // namespace koheron::management
