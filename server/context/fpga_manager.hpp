// Zynq bistream loader
// (c) Koheron

#ifndef __SERVER_CONTEXT_FPGA_MANAGER__
#define __SERVER_CONTEXT_FPGA_MANAGER__

#include <unistd.h>

#include <array>
#include <string>
#include <vector>
#include <sys/mount.h>
#include <experimental/filesystem>
#include <context_base.hpp>
#include "memory.hpp"

namespace {
    namespace fs = std::experimental::filesystem;
}

class FpgaManager {
  public:
    FpgaManager(ContextBase& ctx_)
    : ctx(ctx_)
    {
        if (fs::exists(xdev)) {
            ctx.log<INFO>("Detected xdevcfg\n");
            use_xdevcgf = true;
            return;
        }

        if (fs::exists(fmanager_flags)) {
            ctx.log<INFO>("Detected fmanager\n");

            if (copy_firmware() < 0) {
                ctx.log<PANIC>("Failed to copy firmware\n");
                exit(EXIT_FAILURE);
            }

            if (mount_configfs() < 0) {
                ctx.log<PANIC>("Failed to mount configfs\n");
                exit(EXIT_FAILURE);
            }

            use_overlay = true;
            return;
        }

        ctx.log<PANIC>("Failed to identify bitstream loading mechanism\n");
        exit(EXIT_FAILURE);
    }

    int load_bitstream() {
        ctx.log<INFO>("Loading bitstream...");

        if (use_xdevcgf) {
            return load_bitstream_xdevcfg();
        }

        if (use_overlay) {
           return load_bitstream_overlay();
        }

        return -1;
    }

  private:
    ContextBase& ctx;

    bool use_xdevcgf = false;
    bool use_overlay = false;

    const fs::path live_instrument_dirname = "/tmp/live-instrument/";
    const fs::path xdev = "/dev/xdevcfg";
    const fs::path fmanager_flags = "/sys/class/fpga_manager/fpga0/flags";
    const fs::path overlay_path = "/configfs/device-tree/overlays/full";

    int copy_firmware() {
        const fs::path lib_firmware_dirname = "/lib/firmware/";
        const fs::path bistream_name = instrument_name + std::string(".bit.bin");

        if (!fs::exists(lib_firmware_dirname)) {
            fs::create_directories(lib_firmware_dirname);
        }

        if (!fs::copy_file(live_instrument_dirname / "pl.dtbo",
                           lib_firmware_dirname / "pl.dtbo",
                           fs::copy_options::overwrite_existing)) {
            ctx.log<ERROR>("pl.dtbo copy failed\n");
            return -1;
        }

        if (!fs::copy_file(live_instrument_dirname / bistream_name,
                           lib_firmware_dirname / bistream_name,
                           fs::copy_options::overwrite_existing)) {
            ctx.log<ERROR>("bistream copy failed\n");
            return -1;
        }

        return 0;
    }

    int mount_configfs() {
        if (!fs::exists("/configfs")) {
            fs::create_directories("/configfs");
        }

        if (!fs::exists("/configfs/device-tree/")) {
            if (mount("none", "/configfs", "configfs", 0, nullptr) < 0) {
                return -1;
            }
        }

        if (!fs::exists("/configfs/device-tree/")) {
            return -1;
        }

        // Not permitted to remove /configfs/device-tree/
        // In principle we cannot remove a directory with something mounted on it.

        // remove any previous overlay
        // if (fs::exists(overlay_path)) {
        //     fs::remove_all(overlay_path);
        // }

        // if (fs::exists(overlay_path)) {
        //     ctx.log<PANIC>("Failed to remove previous overlay ...\n");
        // }

        fs::create_directories(overlay_path);

        if (!fs::exists(overlay_path)) {
            return -1;
        }

        return 0;
    }

    int check_bitstream_loaded() {
        FILE *fprog_done = nullptr;
        char expected;

        if (use_xdevcgf) {
            fprog_done = fopen("/sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done", "r");
            expected = '1';
        } else if (use_overlay) {
            fprog_done = fopen("/configfs/device-tree/overlays/full/status", "r");
            expected = 'a';
        } else {
            return -1;
        }

        if (fprog_done == nullptr) {
            ctx.log<PANIC>("FpgaManager: Failed to open FPGA status\n");
            return -1;
        }

        std::array<char, 1> buff{};

        if (read(fileno(fprog_done), buff.data(), 1) == 1) {
            if (buff[0] == expected) {
                ctx.log<INFO>("FpgaManager:Bitstream successfully loaded\n");
                fclose(fprog_done);
                return 0;
            } else {
                ctx.log<PANIC>("FpgaManager: Failed to load bitstream\n");
                fclose(fprog_done);
                return -1;
            }
        } else {
            ctx.log<PANIC>("FpgaManager: Failed to read FPGA status\n");
            fclose(fprog_done);
            return -1;
        }
    }

    int read_bitstream_data(std::vector<char>& bitstream_data) {
        // https://stackoverflow.com/questions/22059189/read-a-file-as-byte-array
        const auto bistream_filename = live_instrument_dirname / fs::path(instrument_name + std::string(".bit"));
        ctx.log<INFO>("FpgaManager: Loading bitstream %s...\n", bistream_filename.c_str());
        FILE *fbitstream = fopen(bistream_filename.c_str(), "rb");

        if (fbitstream == nullptr) {
            ctx.log<PANIC>("FpgaManager: Cannot open bitstream file\n");
            return -1;
        }

        fseek(fbitstream, 0, SEEK_END);
        bitstream_data.resize(ftell(fbitstream));
        rewind(fbitstream);

        if (fread(bitstream_data.data(), bitstream_data.size(), 1, fbitstream) != 1) {
            ctx.log<PANIC>("FpgaManager: Cannot read bitstream data\n");
            fclose(fbitstream);
            return -1;
        }
        
        fclose(fbitstream);
        return 0;
    }

    int load_bitstream_xdevcfg() {
        FILE *xdevcfg = fopen(xdev.c_str(), "w");

        if (xdevcfg == nullptr) {
            ctx.log<PANIC>("FpgaManager: Cannot open xdevcfg\n");
            return -1;
        }

        std::vector<char> bitstream_data{};

        if (read_bitstream_data(bitstream_data) < 0) {
            fclose(xdevcfg);
            return -1;
        }

        if (fwrite(bitstream_data.data(), bitstream_data.size(), 1, xdevcfg) != 1) {
            ctx.log<PANIC>("FpgaManager: Failed to write bitstream to xdevcfg\n");
            fclose(xdevcfg);
            return -1;
        }

        fclose(xdevcfg);
        return check_bitstream_loaded();
    }

    int load_bitstream_overlay() {
        // Set flag to flash entire firmware (i.e. not partial reconfig)
        FILE *xflag = fopen(fmanager_flags.c_str(), "w");

        if (xflag == nullptr) {
            ctx.log<PANIC>("FpgaManager: Cannot open xflag\n");
            return -1;
        }

        ctx.log<INFO>("echo 0 > %s\n", fmanager_flags.c_str());

        if (fwrite("0", 1, 1, xflag) != 1) {
            ctx.log<PANIC>("FpgaManager: Failed to set flag on %s\n", fmanager_flags.c_str());
            fclose(xflag);
            return -1;
        }

        fclose(xflag);

        // Write image
        const fs::path overlay = overlay_path / "path";
        FILE *overlay_file = fopen(overlay.c_str(), "w");

        if (overlay_file == nullptr) {
            ctx.log<PANIC>("FpgaManager: Cannot open overlay\n");
            return -1;
        }

        const std::string echo = "pl.dtbo";

        ctx.log<INFO>("echo %s > %s\n", echo.c_str(), overlay.c_str());

        if (fwrite(echo.c_str(), echo.size(), 1, overlay_file) != 1) {
            ctx.log<PANIC>("FpgaManager: Failed to write bitstream to %s\n", overlay.c_str());
            fclose(overlay_file);
            return -1;
        }

        fclose(overlay_file);
        return check_bitstream_loaded();
    }
}; // FpgaManager

#endif // __SERVER_CONTEXT_FPGA_MANAGER__
