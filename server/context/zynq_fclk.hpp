// Zynq PL Fabric Clocks settings
// (c) Koheron

#ifndef __SERVER_CONTEXT_ZYNQ_FCLK__
#define __SERVER_CONTEXT_ZYNQ_FCLK__

#include <dirent.h>
#include <unistd.h>

#include <cstdio>
#include <string>

#include <context_base.hpp>

class ZynqFclk {
  public:
    ZynqFclk(ContextBase& ctx_)
    : ctx(ctx_)
    {}

    void set(const std::string& fclk_name, uint32_t fclk_rate) {
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

        ctx.log<INFO>("ZynqFclk: Clock %s set to %u Hz\n", fclk_name.c_str(), fclk_rate);
    }

  private:
    ContextBase& ctx;
    const std::string devcfg = "/sys/devices/soc0/amba/f8007000.devcfg";

    int fclk_export(const std::string& fclk_name, const std::string& fclk_dir_name) {
        DIR *fclk_dir = opendir(fclk_dir_name.c_str());

        if (fclk_dir == nullptr) {
            // Clock not exported yet.
            // Call devcfg/fcl_export ....

            const auto fclk_export_name = devcfg + "/fclk_export";
            FILE *fclk_export = fopen(fclk_export_name.c_str(), "w");

            if (fclk_export == nullptr) {
                ctx.log<ERROR>("ZynqFclk: Cannot open fclk_export for clock %s\n", fclk_name.c_str());
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
            ctx.log<ERROR>("ZynqFclk: Cannot open fclk_enable for clock %s\n", fclk_name.c_str());
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

    int fclk_set_rate(const std::string& fclk_name, const std::string& fclk_dir_name, uint32_t fclk_rate) {
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
}; // class ZynqFclk

#endif // __SERVER_CONTEXT_ZYNQ_FCLK__