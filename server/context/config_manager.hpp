#ifndef __SERVER_CONTEXT_CONFIG_MANAGER_HPP__
#define __SERVER_CONTEXT_CONFIG_MANAGER_HPP__

#include "./config_ini.hpp"

#include <filesystem>
#include <string>

namespace {
    namespace fs = std::filesystem;
}

class ConfigManager {
    using Path = fs::path;
  public:
    int init() {
        std::error_code ec;

        fs::create_directories(config_dir, ec);
        if (ec) {
            return -1;
        }

        cfg_.data.clear();

        bool exists = fs::exists(config_path, ec);
        if (ec) {
            return -1;
        }

        if (exists) {
            return cfg::load_ini(config_path, cfg_);
        } else { // Create an empty file and keep cfg_ empty
            std::ofstream out(config_path);
            if (!out) {
                return -1;
            }
        }

        return 0;
    }

    template<class T>
    void set(const std::string& sect, const std::string& key, T value) {
        cfg::set(cfg_, sect, key, value);
    }

    void save() {
        cfg::save_ini(config_path, cfg_);
    }

    bool has(const std::string& sect, const std::string& key) {
        return cfg::has(cfg_, sect, key);
    }

    template<class T>
    T get<>(const std::string& sect, const std::string& key, T deflt = T{}) {
        return cfg::get(cfg_, sect, key, deflt);
    }

  private:
    const Path config_dir = Path("/etc/koheron") / INSTRUMENT_NAME;
    const Path config_path = config_dir / "config.ini";

    cfg::ini cfg_;
};

#endif // __SERVER_CONTEXT_CONFIG_HPP__