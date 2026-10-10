#include "health.hpp"
#include <sstream>
#include <sys/statvfs.h>

namespace koheron::management {
namespace {
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
    add(result, "instrument_service", service_status(settings, settings.unit));
    auto timing = object(); add(timing, "api_ready_monotonic_us", integer(api_ready));
    add(timing, "api_initialization_us", api_ready >= api_started ? integer(api_ready - api_started) : Json{});
    add(timing, "extraction", service_status(settings, "unzip-default-instrument.service"));
    add(result, "timing", std::move(timing)); add(result, "sampled_monotonic_us", integer(monotonic_us()));
    return result;
}
}
