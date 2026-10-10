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
#include <unistd.h>
#include <utility>

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
    Json result(json_tokener_parse_ex(tokener.get(), value.data(), static_cast<int>(value.size())));
    if (json_tokener_get_error(tokener.get()) != json_tokener_success) throw std::runtime_error("Invalid JSON");
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
void service_action(const Settings& settings, std::string_view action, std::string_view unit) {
    if (command({settings.systemctl, std::string(action), std::string(unit)}) != 0)
        throw std::runtime_error("systemctl " + std::string(action) + " " + std::string(unit) + " failed");
}

namespace {
void check(int result) { if (result < 0) throw std::system_error(-result, std::generic_category(), "system bus"); }
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
