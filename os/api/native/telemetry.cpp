#include "telemetry.hpp"
#include <algorithm>
#include <cmath>

namespace koheron::management {
namespace {
json_object* member(json_object* value, const char* name) {
    json_object* result = nullptr;
    if (value && json_object_is_type(value, json_type_object)) json_object_object_get_ex(value, name, &result);
    return result;
}
Json reading(json_object* value, const char* name) {
    auto* item = member(value, name);
    if (!item || (!json_object_is_type(item, json_type_int) && !json_object_is_type(item, json_type_double))) return {};
    const auto number = json_object_get_double(item);
    return std::isfinite(number) ? Json(json_object_new_double(number)) : Json{};
}
Json channels(json_object* source, const char* name, std::initializer_list<const char*> fields) {
    auto result = array();
    auto* values = member(source, name);
    if (!values || !json_object_is_type(values, json_type_array)) return result;
    for (std::size_t i = 0; i < std::min<std::size_t>(json_object_array_length(values), 16); ++i) {
        auto* value = json_object_array_get_idx(values, i);
        auto* label = member(value, "label");
        if (!label || !json_object_is_type(label, json_type_string) || json_object_get_string_len(label) == 0 ||
            json_object_get_string_len(label) > 80) continue;
        auto channel = object(); add(channel, "label", text(json_object_get_string(label)));
        for (const auto* key : fields) add(channel, key, reading(value, key));
        append(result, std::move(channel));
    }
    return result;
}
}
Json board_telemetry(const fs::path& path) {
    auto result = object(); add(result, "state", text("unavailable"));
    add(result, "supplies", array()); add(result, "temperatures", array());
    Json source;
    try {
        const auto bytes = read_file(path, 16384);
        if (!bytes) return result;
        source = parse(*bytes);
    } catch (...) { return result; }
    auto* sampled = member(source.get(), "sampled_monotonic_us");
    if (!sampled || !json_object_is_type(sampled, json_type_int) || json_object_get_int64(sampled) <= 0) return result;
    const auto stamp = json_object_get_uint64(sampled), now = monotonic_us();
    if (stamp > now) return result;
    add(result, "state", text(now - stamp <= 5000000 ? "live" : "stale"));
    add(result, "sampled_monotonic_us", integer(stamp));
    add(result, "supplies", channels(source.get(), "supplies", {"voltage_v", "current_a", "power_w"}));
    add(result, "temperatures", channels(source.get(), "temperatures", {"celsius"}));
    return result;
}
}
