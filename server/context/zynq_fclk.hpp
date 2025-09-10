// Zynq PL Fabric Clocks settings
// (c) Koheron

#ifndef __SERVER_CONTEXT_ZYNQ_FCLK__
#define __SERVER_CONTEXT_ZYNQ_FCLK__

#include <dirent.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <fstream>
#include <iterator>
#include <filesystem>

#include <context_base.hpp>

namespace {
    namespace fs = std::filesystem;
}

class ZynqFclk {
  public:
    ZynqFclk(ContextBase& ctx_)
    : ctx(ctx_)
    {}

    void set(const std::string& fclk_name,
             uint32_t fclk_rate,
             [[maybe_unused]] bool update_rate=false) {
        // When devcfg is used clock rate is always updated.
        //
        // When using devicetree overlay the clock rate is updated when the overlay is loaded.
        // Therefore there is no need to set it in most cases.
        // If we want to update it, set update_rate to true when calling ctx.fclk.set()

        if (fs::exists(devcfg + "/fclk/")) {
            set_fclk_devcfg(fclk_name, fclk_rate);
            return;
        } else {
            const auto clkid = fclk_name.back(); // Ex. if fclk_name = fclk0 then clkid = 0
            const auto clkdir = amba_clocking + clkid;

            if (fs::exists(clkdir)) {
                ctx.log<INFO>("ZynqFclk: Found %s\n", clkdir.c_str());
                set_fclk_amba_clocking(clkdir, clkid, fclk_rate, update_rate);
            } else {
                ctx.log<ERROR>("ZynqFclk: Cannot find %s required to set %s\n",
                               clkdir.c_str(), fclk_name.c_str());
                return;
            }
        }
    }

  private:
    ContextBase& ctx;

    const std::string devcfg = "/sys/devices/soc0/amba/f8007000.devcfg";
    const std::string amba_clocking = "/sys/devices/soc0/axi/axi:clocking";

    // ------------------------------------------------------------------------
    // Use devcfg
    // ------------------------------------------------------------------------

    void set_fclk_devcfg(const std::string& fclk_name, uint32_t fclk_rate) {
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

        ctx.log<INFO>("ZynqFclk: Clock %s set to %u Hz\n",
                      fclk_name.c_str(), fclk_rate);
    }

    int fclk_export(const std::string& fclk_name, const std::string& fclk_dir_name) {
        DIR *fclk_dir = opendir(fclk_dir_name.c_str());

        if (fclk_dir == nullptr) {
            // Clock not exported yet.
            // Call devcfg/fcl_export ....

            const auto fclk_export_name = devcfg + "/fclk_export";
            FILE *fclk_export = fopen(fclk_export_name.c_str(), "w");

            if (fclk_export == nullptr) {
                ctx.log<ERROR>("ZynqFclk: Cannot open fclk_export for clock %s\n",
                               fclk_name.c_str());
                return -1;
            }

            if (write(fileno(fclk_export), fclk_name.c_str(), fclk_name.length() + 1) < 0) {
                ctx.log<ERROR>("ZynqFclk: clock name %s is invalid\n", fclk_name.c_str());
                fclose(fclk_export);
                return -1;
            }

            fclose(fclk_export);
        }

        closedir(fclk_dir);
        return 0;
    }

    int fclk_enable(const std::string& fclk_name, const std::string& fclk_dir_name) {
        const auto fclk_enable_name = fclk_dir_name + "/enable";
        FILE *fclk_enable = fopen(fclk_enable_name.c_str(), "w");

        if (fclk_enable == nullptr) {
            ctx.log<ERROR>("ZynqFclk: Cannot open fclk_enable for clock %s\n",
                           fclk_name.c_str());
            return -1;
        }

        if (write(fileno(fclk_enable), "1", 2) < 0) {
            ctx.log<ERROR>("ZynqFclk: Failed to enable clock %s\n", fclk_name.c_str());
            fclose(fclk_enable);
            return -1;
        }

        fclose(fclk_enable);
        return 0;
    }

    int fclk_set_rate(const std::string& fclk_name,
                      const std::string& fclk_dir_name,
                      uint32_t fclk_rate) {
        const auto fclk_set_rate_name = fclk_dir_name + "/set_rate";
        FILE *fclk_set_rate = fopen(fclk_set_rate_name.c_str(), "w");

        if (fclk_set_rate == nullptr) {
            ctx.log<ERROR>("ZynqFclk: Cannot open fclk_set_rate for clock %s\n", fclk_name.c_str());
            return -1;
        }

        const auto fclk_rate_str = std::to_string(fclk_rate);

        if (write(fileno(fclk_set_rate), fclk_rate_str.c_str(), fclk_rate_str.length() + 1) < 0) {
            ctx.log<ERROR>("ZynqFclk: Failed to set clock %s rate\n", fclk_name.c_str());
            fclose(fclk_set_rate);
            return -1;
        }

        fclose(fclk_set_rate);
        return 0;
    }

    // ------------------------------------------------------------------------
    // Use amba_clocking
    // ------------------------------------------------------------------------

    void set_fclk_amba_clocking(const std::string& clkdir, char clkid,
                                uint32_t fclk_rate, bool update_rate) {
        if (update_rate) {
            amba_clocking_set_rate(clkdir, clkid, fclk_rate);
        }

        const auto rate = amba_clocking_get_rate(clkdir);

        if (rate > 0) {
            ctx.log<INFO>("ZynqFclk: amba:clocking%c rate is %u Hz\n", clkid, rate);

            const auto rel_rate_err =  1E9 * std::abs(rate - long(fclk_rate)) / double(fclk_rate);
            ctx.log<INFO>("ZynqFclk: amba:clocking%c relative rate error is %lf ppb\n",
                          clkid, rel_rate_err);
        }
    }

    int amba_clocking_set_rate(const std::string& clkdir, char clkid, uint32_t fclk_rate) {
        const auto fclk_set_rate_name = clkdir + "/set_rate";
        FILE *file_set_rate = fopen(fclk_set_rate_name.c_str(), "w");

        if (file_set_rate == nullptr) {
            ctx.log<ERROR>("ZynqFclk: Cannot open set_rate for amba:clocking%c\n", clkid);
            return -1;
        }

        const auto fclk_rate_str = std::to_string(fclk_rate);

        if (write(fileno(file_set_rate), fclk_rate_str.c_str(), fclk_rate_str.length() + 1) < 0) {
            ctx.log<ERROR>("ZynqFclk: Failed to set clock for amba:clocking%c\n", clkid);
            fclose(file_set_rate);
            return -1;
        }

        fclose(file_set_rate);
        ctx.log<INFO>("ZynqFclk: amba:clocking%c set to %u Hz\n", clkid, fclk_rate);
        return 0;
    }

    long amba_clocking_get_rate(const std::string& clkdir) {
        const auto fclk_set_rate_name = clkdir + "/set_rate";
        std::ifstream file_set_rate(fclk_set_rate_name);

        if (!file_set_rate.is_open()) {
            ctx.log<ERROR>("ZynqFclk: Cannot open \n", fclk_set_rate_name.c_str());
            return -1;
        }

        std::string set_rate_data(std::istreambuf_iterator<char>{file_set_rate}, {});
        return std::stol(set_rate_data);
    }

}; // class ZynqFclk

#endif // __SERVER_CONTEXT_ZYNQ_FCLK__