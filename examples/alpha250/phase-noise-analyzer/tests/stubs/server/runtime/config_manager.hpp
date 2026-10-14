#pragma once
#include <limits>
#include <server/runtime/config_ini.hpp>
namespace rt {
class ConfigManager {
    cfg::ini data;
public:
    std::error_code save_error;
    template<class T> void set(const std::string& section, const std::string& key, T value) {
        cfg::set(data, section, key, value);
    }
    template<class T> T get(const std::string& section, const std::string& key) {
        return cfg::get<T>(data, section, key, T{});
    }
    bool has(const std::string& section, const std::string& key) { return cfg::has(data, section, key); }
    cfg::Result save_checked() {
        if (save_error) { return std::unexpected(save_error); }
        return {};
    }
    void save() { (void)save_checked(); } // Keep the real INI conversion, without writing /etc/koheron.
};
}
