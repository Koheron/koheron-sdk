#include "archive.hpp"
#include <iostream>

using namespace koheron::management;
int main(int argc, char** argv) {
    try {
        if (argc < 2) throw std::runtime_error("usage: koheron-install ARCHIVE LIVE_DIR [--systemctl PATH] [--unit UNIT] [--led-unit UNIT]\n"
            "       koheron-install --extract-default [--instruments DIR] [--live DIR] [--loader auto|overlay|xdevcfg]");
        Settings settings;
        if (std::string_view(argv[1]) == "--extract-default") {
            std::string_view loader = "auto";
            for (int i = 2; i < argc; i += 2) {
                if (i + 1 >= argc) throw std::runtime_error("Missing option value");
                const std::string_view option(argv[i]);
                if (option == "--instruments") settings.instruments = argv[i + 1];
                else if (option == "--live") settings.live = argv[i + 1];
                else if (option == "--loader") loader = argv[i + 1];
                else throw std::runtime_error("Unknown option " + std::string(option));
            }
            extract_default(settings, loader);
            return 0;
        }
        if (argc < 3) throw std::runtime_error("Missing live instrument directory");
        settings.live = argv[2];
        for (int i = 3; i < argc; i += 2) {
            if (i + 1 >= argc) throw std::runtime_error("Missing option value");
            const std::string_view option(argv[i]);
            if (option == "--systemctl") settings.systemctl = argv[i + 1];
            else if (option == "--unit") settings.unit = argv[i + 1];
            else if (option == "--led-unit") settings.led_unit = argv[i + 1];
            else throw std::runtime_error("Unknown option " + std::string(option));
        }
        install(argv[1], settings);
        return 0;
    } catch (const std::exception& error) { std::cerr << "Instrument installation failed: " << error.what() << '\n'; return 1; }
}
