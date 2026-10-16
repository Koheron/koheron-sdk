#include "health.hpp"
#include "telemetry.hpp"
#include <algorithm>
#include <charconv>
#include <sstream>
#include <sys/statvfs.h>

namespace koheron::management {
namespace {
std::optional<std::string> attribute(const fs::path& path, std::size_t limit = 128) {
    try {
        if (const auto value = read_file(path, limit)) return trim(*value);
    } catch (...) {} // One missing, offline or malformed policy must not hide health.
    return {};
}
std::optional<std::uint64_t> frequency(const fs::path& path) {
    const auto value = attribute(path);
    if (!value) return {};
    std::uint64_t khz = 0;
    const auto parsed = std::from_chars(value->data(), value->data() + value->size(), khz);
    if (parsed.ec != std::errc{} || parsed.ptr != value->data() + value->size() || khz == 0 || khz > 1000000000) return {};
    return khz * 1000;
}
Json cpu(const fs::path& sys) {
    auto result = object(), policies = array();
    std::vector<fs::path> paths;
    std::error_code error;
    const auto root = sys / "devices/system/cpu/cpufreq";
    for (fs::directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        const auto name = it->path().filename().string();
        if (!name.starts_with("policy") || name.size() <= 6 || name.size() > 16 ||
            !std::all_of(name.begin() + 6, name.end(), [](char c) { return c >= '0' && c <= '9'; })) continue;
        paths.push_back(it->path());
        if (paths.size() == 32) break;
    }
    std::sort(paths.begin(), paths.end());
    for (const auto& path : paths) {
        auto policy = object(); add(policy, "label", text(path.filename().string()));
        auto hz = frequency(path / "cpuinfo_cur_freq");
        const bool hardware = hz.has_value();
        if (!hz) hz = frequency(path / "scaling_cur_freq");
        add(policy, "frequency_hz", hz ? integer(*hz) : Json{});
        add(policy, "frequency_source", hz ? text(hardware ? "hardware" : "requested") : Json{});
        add(policy, "governor", text(attribute(path / "scaling_governor", 64).value_or("")));
        add(policy, "affected_cpus", text(attribute(path / "affected_cpus").value_or("")));
        append(policies, std::move(policy));
    }
    add(result, "policies", std::move(policies)); return result;
}
Json storage(fs::path path) {
    auto result = object(); add(result, "path", text(path.string()));
    while (!path.empty() && !fs::exists(path)) path = path.parent_path();
    struct statvfs value{};
    if (path.empty() || ::statvfs(path.c_str(), &value) < 0) { add(result, "error", text("Storage information unavailable")); return result; }
    add(result, "total_bytes", integer(static_cast<std::uint64_t>(value.f_blocks) * value.f_frsize));
    add(result, "available_bytes", integer(static_cast<std::uint64_t>(value.f_bavail) * value.f_frsize));
    return result;
}
}
Json system_health(const Settings& settings, std::uint64_t api_ready, std::uint64_t api_started) {
    auto result = object(), memory = object(), load = array(), disks = object();
    if (const auto bytes = read_file(settings.proc / "uptime", 256)) {
        std::istringstream input(*bytes); double uptime = 0;
        if (input >> uptime && uptime >= 0) add(result, "uptime_seconds", integer(static_cast<std::uint64_t>(uptime)));
    }
    if (const auto bytes = read_file(settings.proc / "loadavg", 256)) {
        std::istringstream input(*bytes); double value = 0;
        for (int i = 0; i < 3 && (input >> value); ++i) append(load, Json(json_object_new_double(value)));
    }
    if (const auto bytes = read_file(settings.proc / "meminfo", 16384)) {
        std::istringstream lines(*bytes); std::string line;
        while (std::getline(lines, line)) {
            std::istringstream input(line); std::string key, unit; std::uint64_t value = 0;
            if (!(input >> key >> value >> unit) || unit != "kB") continue;
            if (key == "MemTotal:") add(memory, "total_bytes", integer(value * 1024));
            if (key == "MemAvailable:") add(memory, "available_bytes", integer(value * 1024));
        }
    }
    add(disks, "instruments", storage(settings.instruments)); add(disks, "staging", storage(settings.live.parent_path()));
    add(result, "load_average", std::move(load)); add(result, "memory", std::move(memory)); add(result, "storage", std::move(disks));
    add(result, "cpu", cpu(settings.sys));
    add(result, "instrument_service", service_status(settings, settings.unit));
    add(result, "telemetry", board_telemetry(settings.telemetry));
    auto timing = object(); add(timing, "api_ready_monotonic_us", integer(api_ready));
    add(timing, "api_initialization_us", api_ready >= api_started ? integer(api_ready - api_started) : Json{});
    add(timing, "extraction", service_status(settings, "unzip-default-instrument.service"));
    add(result, "timing", std::move(timing)); add(result, "sampled_monotonic_us", integer(monotonic_us()));
    return result;
}
}
