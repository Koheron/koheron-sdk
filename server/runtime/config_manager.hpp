#ifndef __SERVER_CONTEXT_CONFIG_MANAGER_HPP__
#define __SERVER_CONTEXT_CONFIG_MANAGER_HPP__

#include "./config_ini.hpp"
#include "./syslog.hpp"

#include <filesystem>
#include <string>

namespace rt {

namespace fs = std::filesystem;

class ConfigManager {
    using Path = fs::path;
  public:
    int init() {
        std::error_code ec;

        fs::create_directories(config_dir, ec);
        if (ec) {
            return report_error("create directory", ec);
        }

        cfg_.data.clear();

        bool exists = fs::exists(config_path, ec);
        if (ec) {
            return report_error("check file", ec);
        }

        if (exists) {
            const auto result = cfg::load_ini(config_path, cfg_);
            return result ? 0 : report_error("load", result.error());
        } else { // Create an empty file and keep cfg_ empty
            std::ofstream out(config_path);
            out.close();
            if (!out) {
                return report_error("create file", std::make_error_code(std::io_errc::stream));
            }
        }

        return 0;
    }

    template<class T>
    void set(const std::string& sect, const std::string& key, T value) {
        cfg::set(cfg_, sect, key, value);
    }

    void save() {
        if (const auto result = cfg::save_ini(config_path, cfg_); !result) {
            report_error("save", result.error());
        }
    }

    bool has(const std::string& sect, const std::string& key) {
        return cfg::has(cfg_, sect, key);
    }

    template<class T>
    T get(const std::string& sect, const std::string& key, T deflt = T{}) {
        return cfg::get<T>(cfg_, sect, key, deflt);
    }

  private:
    int report_error(std::string_view operation, const std::error_code& error) const {
        logf<ERROR>("Configuration {} failed for {}: {}\n", operation, config_path, error.message());
        return -1;
    }

    const Path config_dir = Path("/etc/koheron") / INSTRUMENT_NAME;
    const Path config_path = config_dir / "config.ini";

    cfg::ini cfg_;
};

} // namespace rt

#endif // __SERVER_CONTEXT_CONFIG_HPP__
