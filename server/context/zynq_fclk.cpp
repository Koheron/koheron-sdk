
#include "server/context/zynq_fclk.hpp"

#include "server/runtime/syslog.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <filesystem>

#include <dirent.h>
#include <unistd.h>

namespace fs = std::filesystem;

void ZynqFclk::set(const std::string& fclk_name,
            uint32_t fclk_rate,
            [[maybe_unused]] bool update_rate) {


    if (fs::exists(devcfg + "/fclk/")) {
        set_fclk_devcfg(fclk_name, fclk_rate);
        return;
    } else {
        const auto clkid = fclk_name.back(); // Ex. if fclk_name = fclk0 then clkid = 0
        const auto clkdir = amba_clocking + clkid;

        if (fs::exists(clkdir)) {
            koheron::print_fmt<INFO>("ZynqFclk: Found {}\n", clkdir);
            set_fclk_amba_clocking(clkdir, clkid, fclk_rate, update_rate);
        } else {
            koheron::print_fmt<ERROR>("ZynqFclk: Cannot find {} required to set {}\n", clkdir, fclk_name);
            return;
        }
    }
}

// ------------------------------------------------------------------------
// Use devcfg
// ------------------------------------------------------------------------

void ZynqFclk::set_fclk_devcfg(const std::string& fclk_name, uint32_t fclk_rate) {
    const auto fclk_dir_name = devcfg + "/fclk/" + fclk_name;

    if (fclk_export(fclk_name, fclk_dir_name) < 0) {
        return;
    }

    if (fclk_enable(fclk_name, fclk_dir_name) < 0) {
        return;
    }

    if (fclk_set_rate(fclk_name, fclk_dir_name, fclk_rate) < 0) {
        return;
    }

    koheron::print_fmt<INFO>("ZynqFclk: Clock {} set to {} Hz\n", fclk_name, fclk_rate);
}

int ZynqFclk::fclk_export(const std::string& fclk_name, const std::string& fclk_dir_name) {
    DIR *fclk_dir = opendir(fclk_dir_name.c_str());

    if (fclk_dir == nullptr) {
        // Clock not exported yet.
        // Call devcfg/fcl_export ....

        const auto fclk_export_name = devcfg + "/fclk_export";
        FILE *fclk_export = fopen(fclk_export_name.c_str(), "w");

        if (fclk_export == nullptr) {
            koheron::print_fmt<ERROR>("ZynqFclk: Cannot open fclk_export for clock {}\n", fclk_name);
            return -1;
        }

        if (write(fileno(fclk_export), fclk_name.c_str(), fclk_name.length() + 1) < 0) {
            koheron::print_fmt<ERROR>("ZynqFclk: clock name {} is invalid\n", fclk_name);
            fclose(fclk_export);
            return -1;
        }

        fclose(fclk_export);
    }

    closedir(fclk_dir);
    return 0;
}

int ZynqFclk::fclk_enable(const std::string& fclk_name, const std::string& fclk_dir_name) {
    const auto fclk_enable_name = fclk_dir_name + "/enable";
    FILE *fclk_enable = fopen(fclk_enable_name.c_str(), "w");

    if (fclk_enable == nullptr) {
        koheron::print_fmt<ERROR>("ZynqFclk: Cannot open fclk_enable for clock {}\n", fclk_name);
        return -1;
    }

    if (write(fileno(fclk_enable), "1", 2) < 0) {
        koheron::print_fmt<ERROR>("ZynqFclk: Failed to enable clock {}\n", fclk_name);
        fclose(fclk_enable);
        return -1;
    }

    fclose(fclk_enable);
    return 0;
}

int ZynqFclk::fclk_set_rate(const std::string& fclk_name,
                            const std::string& fclk_dir_name,
                            uint32_t fclk_rate) {
    const auto fclk_set_rate_name = fclk_dir_name + "/set_rate";
    FILE *fclk_set_rate = fopen(fclk_set_rate_name.c_str(), "w");

    if (fclk_set_rate == nullptr) {
        koheron::print_fmt<ERROR>("ZynqFclk: Cannot open fclk_set_rate for clock {}\n", fclk_name);
        return -1;
    }

    const auto fclk_rate_str = std::to_string(fclk_rate);

    if (write(fileno(fclk_set_rate), fclk_rate_str.c_str(), fclk_rate_str.length() + 1) < 0) {
        koheron::print_fmt<ERROR>("ZynqFclk: Failed to set clock {} rate\n", fclk_name);
        fclose(fclk_set_rate);
        return -1;
    }

    fclose(fclk_set_rate);
    return 0;
}

// ------------------------------------------------------------------------
// Use amba_clocking
// ------------------------------------------------------------------------

void ZynqFclk::set_fclk_amba_clocking(const std::string& clkdir, char clkid,
                                      uint32_t fclk_rate, bool update_rate) {
    if (update_rate) {
        amba_clocking_set_rate(clkdir, clkid, fclk_rate);
    }

    const auto rate = amba_clocking_get_rate(clkdir);

    if (rate > 0) {
        koheron::print_fmt<INFO>("ZynqFclk: amba:clocking{} rate is {} Hz\n", clkid, rate);
    }
}

int ZynqFclk::amba_clocking_set_rate(const std::string& clkdir, char clkid, uint32_t fclk_rate) {
    const auto fclk_set_rate_name = clkdir + "/set_rate";
    FILE *file_set_rate = fopen(fclk_set_rate_name.c_str(), "w");

    if (file_set_rate == nullptr) {
        koheron::print_fmt<ERROR>("ZynqFclk: Cannot open set_rate for amba:clocking{}\n", clkid);
        return -1;
    }

    const auto fclk_rate_str = std::to_string(fclk_rate);

    if (write(fileno(file_set_rate), fclk_rate_str.c_str(), fclk_rate_str.length() + 1) < 0) {
        koheron::print_fmt<ERROR>("ZynqFclk: Failed to set clock for amba:clocking{}\n", clkid);
        fclose(file_set_rate);
        return -1;
    }

    fclose(file_set_rate);
    koheron::print_fmt<INFO>("ZynqFclk: amba:clocking{} set to {} Hz\n", clkid, fclk_rate);
    return 0;
}

long ZynqFclk::amba_clocking_get_rate(const std::string& clkdir) {
    const auto fclk_set_rate_name = clkdir + "/set_rate";
    std::ifstream file_set_rate(fclk_set_rate_name);

    if (!file_set_rate.is_open()) {
        koheron::print_fmt<ERROR>("ZynqFclk: Cannot open {}\n", fclk_set_rate_name);
        return -1;
    }

    std::string set_rate_data(std::istreambuf_iterator<char>{file_set_rate}, {});
    return std::stol(set_rate_data);
}