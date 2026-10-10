#include "common.hpp"

#include <systemd/sd-bus.h>
#include <uninorm.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <map>
#include <time.h>

extern char** environ;

namespace koheron::management {
Fd::~Fd() { if (value_ >= 0) ::close(value_); }
Fd::Fd(Fd&& other) noexcept : value_(other.release()) {}
Fd& Fd::operator=(Fd&& other) noexcept {
    if (this != &other) { if (value_ >= 0) ::close(value_); value_ = other.release(); }
    return *this;
}
int Fd::release() noexcept { return std::exchange(value_, -1); }
Json object() { return Json(json_object_new_object()); }
Json array() { return Json(json_object_new_array()); }
Json text(std::string_view value) {
    const auto bytes = utf8(value);
    return Json(json_object_new_string_len(bytes.data(), static_cast<int>(bytes.size())));
}
Json boolean(bool value) { return Json(json_object_new_boolean(value)); }
Json integer(std::uint64_t value) { return Json(json_object_new_uint64(value)); }
void add(Json& target, std::string_view key, Json value) { json_object_object_add(target.get(), std::string(key).c_str(), value.release()); }
void append(Json& target, Json value) { json_object_array_add(target.get(), value.release()); }
std::string encode(const Json& value) { return json_object_to_json_string_ext(value.get(), JSON_C_TO_STRING_PLAIN); }
Json parse(std::string_view value) {
    std::unique_ptr<json_tokener, decltype(&json_tokener_free)> tokener(json_tokener_new(), json_tokener_free);
    json_tokener_set_flags(tokener.get(), JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
    Json result(json_tokener_parse_ex(tokener.get(), value.data(), static_cast<int>(value.size())));
    if (json_tokener_get_error(tokener.get()) != json_tokener_success ||
        value.substr(json_tokener_get_parse_end(tokener.get())).find_first_not_of(" \t\r\n") != value.npos) throw std::runtime_error("Invalid JSON");
    return result;
}
std::string field(json_object* value, const char* key) {
    json_object* item = nullptr;
    return json_object_object_get_ex(value, key, &item) && json_object_is_type(item, json_type_string)
        ? json_object_get_string(item) : "";
}
std::string trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n\v\f");
    if (first == value.npos) return {};
    return std::string(value.substr(first, value.find_last_not_of(" \t\r\n\v\f") - first + 1));
}
std::string utf8(std::string_view value) {
    std::string result;
    while (!value.empty()) {
        const auto lead = static_cast<unsigned char>(value.front());
        std::size_t length = lead < 0x80 ? 1 : lead >= 0xc2 && lead <= 0xdf ? 2 :
            lead >= 0xe0 && lead <= 0xef ? 3 : lead >= 0xf0 && lead <= 0xf4 ? 4 : 0;
        bool valid = length != 0;
        std::size_t consumed = 1;
        for (std::size_t i = 1; valid && i < length; ++i) {
            if (i >= value.size()) { valid = false; break; }
            const auto byte = static_cast<unsigned char>(value[i]);
            valid = byte >= 0x80 && byte <= 0xbf;
            if (i == 1) valid = valid && !(lead == 0xe0 && byte < 0xa0) && !(lead == 0xed && byte >= 0xa0) &&
                !(lead == 0xf0 && byte < 0x90) && !(lead == 0xf4 && byte >= 0x90);
            if (valid) ++consumed;
        }
        if (valid) result.append(value.substr(0, length));
        else { result += "\xef\xbf\xbd"; length = consumed; }
        value.remove_prefix(length);
    }
    return result;
}
std::optional<std::string> read_file(const fs::path& path, std::size_t limit) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    std::string result;
    std::array<char, 8192> buffer{};
    while (file) {
        file.read(buffer.data(), buffer.size());
        const auto count = static_cast<std::size_t>(file.gcount());
        if (count > limit - result.size()) throw std::runtime_error("File exceeds size limit: " + path.string());
        result.append(buffer.data(), count);
    }
    if (!file.eof()) throw std::runtime_error("Could not read " + path.string());
    return result;
}
void write_all(int fd, std::span<const char> bytes) {
    while (!bytes.empty()) {
        const auto count = ::write(fd, bytes.data(), bytes.size());
        if (count < 0) { if (errno == EINTR) continue; system_error("write"); }
        if (count == 0) throw std::runtime_error("Short write");
        bytes = bytes.subspan(static_cast<std::size_t>(count));
    }
}
void write_file(const fs::path& path, std::string_view bytes, unsigned mode) {
    Fd fd(::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, mode));
    if (fd.get() < 0) system_error("open " + path.string());
    write_all(fd.get(), std::span(bytes.data(), bytes.size()));
}
fs::path temporary_directory(const fs::path& parent, std::string_view prefix) {
    std::string pattern = (parent / (std::string(prefix) + "XXXXXX")).string();
    if (!::mkdtemp(pattern.data())) system_error("mkdtemp");
    return pattern;
}
std::pair<fs::path, Fd> temporary_file(const fs::path& parent, std::string_view prefix) {
    std::string pattern = (parent / (std::string(prefix) + "XXXXXX")).string();
    Fd fd(::mkostemp(pattern.data(), O_CLOEXEC));
    if (fd.get() < 0) system_error("mkostemp");
    return {pattern, std::move(fd)};
}
std::string safe_filename(std::string_view filename) {
    // Match Werkzeug's NFKD -> ASCII -> whitespace joining -> character filter.
    const auto input = utf8(filename);
    std::size_t length = 0;
    std::unique_ptr<std::uint8_t, decltype(&std::free)> normalized(
        u8_normalize(UNINORM_NFKD, reinterpret_cast<const std::uint8_t*>(input.data()), input.size(), nullptr, &length), std::free);
    if (!normalized) system_error("filename normalization");
    std::string result;
    bool token = false, separator = false;
    for (const auto c : std::span(normalized.get(), length)) {
        if (c >= 0x80) continue;
        if (c == '/' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
            separator = token; continue;
        }
        if (separator) { result += '_'; separator = false; }
        token = true;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.') result += static_cast<char>(c);
    }
    const auto begin = result.find_first_not_of("._");
    if (begin == result.npos) return {};
    return result.substr(begin, result.find_last_not_of("._") - begin + 1);
}
std::uint64_t realtime_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
std::uint64_t monotonic_us() {
    timespec value{};
    if (::clock_gettime(CLOCK_MONOTONIC, &value) < 0) system_error("monotonic clock");
    return static_cast<std::uint64_t>(value.tv_sec) * 1000000 + value.tv_nsec / 1000;
}
void sync_directory(const fs::path& path) {
    Fd directory(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (directory.get() < 0 || ::fsync(directory.get()) < 0) system_error("sync directory");
}
void atomic_write(const fs::path& path, std::string_view bytes) {
    auto [temporary, fd] = temporary_file(path.parent_path(), ".preference-");
    try {
        write_all(fd.get(), std::span(bytes.data(), bytes.size()));
        if (::fchmod(fd.get(), 0644) < 0 || ::fsync(fd.get()) < 0) system_error("sync preference");
        fs::rename(temporary, path);
        sync_directory(path.parent_path());
    } catch (...) { std::error_code ignored; fs::remove(temporary, ignored); throw; }
}
Json kv_file(const fs::path& path) {
    const auto bytes = read_file(path, 65536);
    if (!bytes) return {};
    auto result = object();
    std::string_view remaining(*bytes);
    while (!remaining.empty()) {
        const auto end = remaining.find('\n');
        const auto line = trim(remaining.substr(0, end));
        if (!line.empty() && !line.starts_with('#')) {
            const auto equals = line.find('=');
            if (equals != line.npos) add(result, trim(std::string_view(line).substr(0, equals)), text(trim(std::string_view(line).substr(equals + 1))));
        }
        if (end == remaining.npos) break;
        remaining.remove_prefix(end + 1);
    }
    return result;
}
void system_error(std::string_view context) { throw std::system_error(errno, std::generic_category(), std::string(context)); }
int command(const std::vector<std::string>& arguments) {
    std::vector<char*> argv;
    for (const auto& argument : arguments) argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);
    struct Attributes {
        posix_spawnattr_t value;
        Attributes() {
            const int error = ::posix_spawnattr_init(&value);
            if (error) throw std::system_error(error, std::generic_category(), "posix_spawnattr_init");
        }
        ~Attributes() { ::posix_spawnattr_destroy(&value); }
    } attributes;
    sigset_t mask;
    sigemptyset(&mask);
    ::posix_spawnattr_setsigmask(&attributes.value, &mask);
    ::posix_spawnattr_setflags(&attributes.value, POSIX_SPAWN_SETSIGMASK);
    pid_t pid = 0;
    const int error = ::posix_spawn(&pid, argv.front(), nullptr, &attributes.value, argv.data(), environ);
    if (error) throw std::system_error(error, std::generic_category(), "posix_spawn");
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) { if (errno != EINTR) system_error("waitpid"); }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
namespace {
void check(int result) { if (result < 0) throw std::system_error(-result, std::generic_category(), "system bus"); }
struct Bus {
    std::unique_ptr<sd_bus, decltype(&sd_bus_close_unref)> value{nullptr, sd_bus_close_unref};
    Bus() {
        sd_bus* raw = nullptr; check(sd_bus_open_system(&raw)); value.reset(raw);
        check(sd_bus_set_method_call_timeout(raw, 5000000));
        const auto deadline = monotonic_us() + 5000000;
        int ready = 0;
        while ((ready = sd_bus_is_ready(raw)) == 0) {
            if (monotonic_us() >= deadline) throw std::runtime_error("System bus connection timed out");
            const int processed = sd_bus_process(raw, nullptr); check(processed);
            if (!processed) check(sd_bus_wait(raw, 100000));
        }
        check(ready);
    }
};
std::string unit_path(std::string_view unit) {
    char* raw = nullptr;
    check(sd_bus_path_encode("/org/freedesktop/systemd1/unit", std::string(unit).c_str(), &raw));
    std::unique_ptr<char, decltype(&std::free)> path(raw, std::free);
    return path.get();
}
std::string property(sd_bus* bus, const std::string& path, const char* interface, const char* key) {
    char* raw = nullptr;
    const int result = sd_bus_get_property_string(bus, "org.freedesktop.systemd1", path.c_str(), interface, key, nullptr, &raw);
    std::unique_ptr<char, decltype(&std::free)> value(raw, std::free);
    check(result); return value.get();
}
}
void service_action(const Settings& settings, std::string_view action, std::string_view unit) {
    if (settings.systemctl != "/bin/systemctl") {
        if (command({settings.systemctl, std::string(action), std::string(unit)}) != 0)
            throw std::runtime_error("Service " + std::string(action) + " " + std::string(unit) + " failed");
        return;
    }
    const char* method = action == "start" ? "StartUnit" : action == "stop" ? "StopUnit" : action == "restart" ? "RestartUnit" : nullptr;
    if (!method) throw std::runtime_error("Unsupported service action");
    Bus bus;
    struct Jobs { std::string unit; std::map<std::string, std::string> results; } jobs{std::string(unit), {}};
    sd_bus_slot* raw_slot = nullptr;
    check(sd_bus_match_signal(bus.value.get(), &raw_slot, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "JobRemoved", [](sd_bus_message* message, void* context, sd_bus_error*) -> int {
            auto& jobs = *static_cast<Jobs*>(context);
            std::uint32_t id = 0; const char *path = nullptr, *unit = nullptr, *result = nullptr;
            const int read = sd_bus_message_read(message, "uoss", &id, &path, &unit, &result);
            if (read < 0) return read;
            if (jobs.unit == unit && jobs.results.size() < 64) jobs.results[path] = result;
            return 0;
        }, &jobs));
    std::unique_ptr<sd_bus_slot, decltype(&sd_bus_slot_unref)> slot(raw_slot, sd_bus_slot_unref);
    check(sd_bus_call_method(bus.value.get(), "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "Subscribe", nullptr, nullptr, ""));
    sd_bus_message* raw_reply = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    const int called = sd_bus_call_method(bus.value.get(), "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", method, &error, &raw_reply, "ss", std::string(unit).c_str(), "replace");
    std::unique_ptr<sd_bus_message, decltype(&sd_bus_message_unref)> reply(raw_reply, sd_bus_message_unref);
    const std::string detail = error.message ? error.message : "Systemd job could not be queued";
    sd_bus_error_free(&error);
    if (called < 0) throw std::runtime_error(detail);
    const char* raw_path = nullptr; check(sd_bus_message_read(reply.get(), "o", &raw_path));
    const std::string path(raw_path);
    const auto deadline = monotonic_us() + 120000000;
    while (!jobs.results.contains(path)) {
        if (monotonic_us() >= deadline) {
            (void)sd_bus_call_method(bus.value.get(), "org.freedesktop.systemd1", path.c_str(),
                "org.freedesktop.systemd1.Job", "Cancel", nullptr, nullptr, "");
            throw std::runtime_error("Systemd " + std::string(action) + " job timed out");
        }
        const int processed = sd_bus_process(bus.value.get(), nullptr); check(processed);
        if (!processed) check(sd_bus_wait(bus.value.get(), 100000));
    }
    if (jobs.results.at(path) != "done") throw std::runtime_error("Systemd " + std::string(action) + " failed: " + jobs.results.at(path));
}
Json service_status(const Settings& settings, std::string_view unit) {
    auto result = object(); add(result, "unit", text(unit));
    try {
        if (settings.systemctl != "/bin/systemctl") {
            if (unit != settings.unit) return result;
            add(result, "state", text(unit_is_active(settings) ? "active" : "inactive"));
            return result;
        }
        Bus bus; const auto path = unit_path(unit);
        constexpr auto manager = "org.freedesktop.systemd1.Unit";
        constexpr auto service = "org.freedesktop.systemd1.Service";
        add(result, "state", text(property(bus.value.get(), path, manager, "ActiveState")));
        add(result, "substate", text(property(bus.value.get(), path, manager, "SubState")));
        add(result, "result", text(property(bus.value.get(), path, service, "Result")));
        for (const auto* key : {"ActiveEnterTimestampMonotonic", "InactiveExitTimestampMonotonic"}) {
            std::uint64_t value = 0;
            if (sd_bus_get_property_trivial(bus.value.get(), "org.freedesktop.systemd1", path.c_str(), manager, key, nullptr, 't', &value) >= 0)
                add(result, key, integer(value));
        }
        for (const auto* key : {"ExecMainStartTimestampMonotonic", "ExecMainExitTimestampMonotonic"}) {
            std::uint64_t value = 0;
            if (sd_bus_get_property_trivial(bus.value.get(), "org.freedesktop.systemd1", path.c_str(), service, key, nullptr, 't', &value) >= 0)
                add(result, key, integer(value));
        }
        std::uint32_t restarts = 0; std::int32_t status = 0;
        if (sd_bus_get_property_trivial(bus.value.get(), "org.freedesktop.systemd1", path.c_str(), service, "NRestarts", nullptr, 'u', &restarts) >= 0)
            add(result, "restarts", integer(restarts));
        if (sd_bus_get_property_trivial(bus.value.get(), "org.freedesktop.systemd1", path.c_str(), service, "ExecMainStatus", nullptr, 'i', &status) >= 0)
            add(result, "exit_status", integer(static_cast<std::uint32_t>(status)));
    } catch (const std::exception& error) { add(result, "error", text(error.what())); }
    return result;
}
bool unit_is_active(const Settings& settings) {
    if (settings.systemctl != "/bin/systemctl") return command({settings.systemctl, "is-active", "--quiet", settings.unit}) == 0;
    sd_bus* raw_bus = nullptr;
    const int opened = sd_bus_open_system(&raw_bus);
    std::unique_ptr<sd_bus, decltype(&sd_bus_close_unref)> bus(raw_bus, sd_bus_close_unref);
    check(opened);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    const auto remaining = [&] {
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (us <= 0) throw std::system_error(ETIMEDOUT, std::generic_category(), "system bus");
        return static_cast<std::uint64_t>(us);
    };
    int ready = 0;
    while ((ready = sd_bus_is_ready(bus.get())) == 0) {
        remaining();
        const int processed = sd_bus_process(bus.get(), nullptr);
        check(processed);
        if (processed == 0) check(sd_bus_wait(bus.get(), remaining()));
    }
    check(ready);
    char* raw_path = nullptr;
    check(sd_bus_path_encode("/org/freedesktop/systemd1/unit", settings.unit.c_str(), &raw_path));
    std::unique_ptr<char, decltype(&std::free)> path(raw_path, std::free);
    check(sd_bus_set_method_call_timeout(bus.get(), remaining()));
    char* raw_state = nullptr;
    const int result = sd_bus_get_property_string(bus.get(), "org.freedesktop.systemd1", path.get(),
        "org.freedesktop.systemd1.Unit", "ActiveState", nullptr, &raw_state);
    std::unique_ptr<char, decltype(&std::free)> state(raw_state, std::free);
    check(result);
    const std::string_view value(state.get());
    return value == "active" || value == "reloading" || value == "refreshing";
}
} // namespace koheron::management
