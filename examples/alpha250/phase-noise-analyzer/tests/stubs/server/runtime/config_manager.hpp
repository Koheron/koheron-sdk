#pragma once
#include <limits>
#include <server/runtime/config_ini.hpp>
namespace rt {
class ConfigManager {
    cfg::ini data;
public:
    template<class T> void set(const std::string& section, const std::string& key, T value) {
        cfg::set(data, section, key, value);
    }
    template<class T> T get(const std::string& section, const std::string& key) {
        return cfg::get<T>(data, section, key, T{});
    }
    bool has(const std::string& section, const std::string& key) { return cfg::has(data, section, key); }
    void save() {} // Keep the real INI conversion, without writing /etc/koheron.
};
}
