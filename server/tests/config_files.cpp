// Exercise expected-based configuration errors with exceptions disabled.
#include "server/runtime/config_ini.hpp"
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) { return 2; }
    rt::cfg::ini config;
    rt::cfg::Result result;
    if (std::string_view(argv[1]) == "save") {
        rt::cfg::set(config, "instrument", "enabled", true);
        rt::cfg::set(config, "instrument", "rate", 125000000);
        result = rt::cfg::save_ini(argv[2], config);
    } else {
        result = rt::cfg::load_ini(argv[2], config);
        if (result) {
            if (!rt::cfg::get<bool>(config, "instrument", "enabled", false) ||
                rt::cfg::get<int>(config, "instrument", "rate", 0) != 125000000) {
                return 3;
            }
        }
    }
    if (!result) {
        std::cerr << result.error().category().name() << ':' << result.error().value()
                  << ':' << result.error().message() << '\n';
        return 1;
    }
}
