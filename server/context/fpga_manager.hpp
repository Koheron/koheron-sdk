// Zynq bistream loader
// (c) Koheron

#ifndef __SERVER_CONTEXT_FPGA_MANAGER__
#define __SERVER_CONTEXT_FPGA_MANAGER__

#include <unistd.h>

#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <sys/mount.h>

#include <context_base.hpp>
#include "memory.hpp"

#define str_(s) #s
#define xstr_(s) str_(s)

namespace {
    namespace fs = std::filesystem;
}

class FpgaManager {
  public:
    FpgaManager(ContextBase& ctx_)
    : ctx(ctx_)
    {
        if (fs::exists(xdev)) {
            ctx.log<INFO>("FpgaManager: Bitstream loading method: xdevcfg\n");
            use_xdevcgf = true;
            return;
        }

        if (fs::exists(fmanager_flags)) {
            ctx.log<INFO>("FpgaManager: Bitstream loading method: fpga_manager\n");
            use_overlay = true;
            return;
        }

        ctx.log<PANIC>("FpgaManager: Failed to identify bitstream loading mechanism\n");
        exit(EXIT_FAILURE);
    }

    int load_bitstream() {
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
    const std::string instrument_name = xstr_(INSTRUMENT_NAME);

    bool use_xdevcgf = false;
    bool use_overlay = false;

    const fs::path live_instrument_dirname = "/tmp/live-instrument/";

    int check_bitstream_loaded(const fs::path& fprog_done_path, char expected) {
        FILE *fprog_done =fopen(fprog_done_path.c_str(), "r");

        if (fprog_done == nullptr) {
            ctx.log<PANIC>("FpgaManager: Failed to open FPGA status\n");
            return -1;
        }

        std::array<char, 1> buff{};

        if (read(fileno(fprog_done), buff.data(), 1) == 1) {
            if (buff[0] == expected) {
                ctx.log<INFO>("FpgaManager: Bitstream successfully loaded\n");
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

    // ------------------------------------------------------------------------
    // Load using overlay
    // ------------------------------------------------------------------------

    const fs::path fmanager_flags = "/sys/class/fpga_manager/fpga0/flags";
    const fs::path overlay_path = "/configfs/device-tree/overlays/full";
    const fs::path overlay_fpga_done = "/configfs/device-tree/overlays/full/status";

    int copy_firmware() {
        const fs::path lib_firmware_dirname = "/lib/firmware/";
        const fs::path bistream_name = instrument_name + std::string(".bit.bin");
        ctx.logf<INFO>("FpgaManager: Loading bitstream {}...\n", bistream_name);

        if (!fs::exists(lib_firmware_dirname)) {
            fs::create_directories(lib_firmware_dirname);
        }

        if (!fs::copy_file(live_instrument_dirname / "pl.dtbo",
                           lib_firmware_dirname / "pl.dtbo",
                           fs::copy_options::overwrite_existing)) {
            ctx.log<ERROR>("FpgaManager: pl.dtbo copy failed\n");
            return -1;
        }

        if (!fs::copy_file(live_instrument_dirname / bistream_name,
                           lib_firmware_dirname / bistream_name,
                           fs::copy_options::overwrite_existing)) {
            ctx.log<ERROR>("FpgaManager: bistream copy failed\n");
            return -1;
        }

        return 0;
    }

    int clean_up_previous_overlays() {
        if (fs::exists(overlay_path)) {
            // In the device-tree overlay configfs, you’re supposed to remove an overlay by rmdir on the overlay directory.
            // You’re not allowed to unlink the files inside (like dtbo). Kernel refuses that with EPERM (“Operation not permitted”).
            // std::filesystem::remove_all tries to recursively unlink everything inside before removing the directory, so it hits the dtbo file and throws filesystem_error.
            // So we cannot use fs::remove_all(overlay_path) here.
            ::rmdir(overlay_path.c_str());
        }

        if (fs::exists(overlay_path)) {
            ctx.log<PANIC>("FpgaManager: Failed to remove previous overlay\n");
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

        return 0;
    }

    int setup_overlay_path() {
        if (mount_configfs() < 0) {
            return -1;
        }

        if (clean_up_previous_overlays() < 0) {
            return -1;
        }

        fs::create_directories(overlay_path);

        if (!fs::exists(overlay_path)) {
            return -1;
        }

        return 0;
    }

    int setup_fmanager_flags() {
        // Set flag to flash entire firmware (i.e. not partial reconfig)
        FILE *xflag = fopen(fmanager_flags.c_str(), "w");

        if (xflag == nullptr) {
            ctx.log<ERROR>("FpgaManager: Cannot open xflag\n");
            return -1;
        }

        if (fwrite("0", 1, 1, xflag) != 1) {
            fclose(xflag);
            return -1;
        }

        fclose(xflag);
        return 0;
    }

    int write_overlay() {
        const fs::path overlay = overlay_path / "path";
        FILE *overlay_file = fopen(overlay.c_str(), "w");

        if (overlay_file == nullptr) {
            ctx.log<ERROR>("FpgaManager: Cannot open overlay\n");
            return -1;
        }

        const std::string echo = "pl.dtbo\n";

        if (fwrite(echo.c_str(), echo.size(), 1, overlay_file) != 1) {
            fclose(overlay_file);
            return -1;
        }

        fclose(overlay_file);
        return 0;
    }

    int load_bitstream_overlay() {
        if (copy_firmware() < 0) {
            ctx.log<ERROR>("FpgaManager: Failed to copy firmware\n");
            return -1;
        }

        if (setup_overlay_path() < 0) {
            ctx.log<ERROR>("FpgaManager: Failed to setup overlay path\n");
            return -1;
        }

        if (setup_fmanager_flags() < 0) {
            ctx.logf<ERROR>("FpgaManager: Failed to set flag on {}\n", fmanager_flags);
            return -1;
        }

        if (write_overlay() < 0) {
            ctx.logf<ERROR>("FpgaManager: Failed to write bitstream to {}\n", overlay_path);
            return -1;
        }

        return check_bitstream_loaded(overlay_fpga_done, 'a');
    }

    // ------------------------------------------------------------------------
    // Load using xdevcfg
    // ------------------------------------------------------------------------

    const fs::path xdev = "/dev/xdevcfg";
    const fs::path xdev_fpga_done = "/sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done";

    int read_bitstream_data(std::vector<char>& bitstream_data) {
        // https://stackoverflow.com/questions/22059189/read-a-file-as-byte-array
        const auto bistream_basename = fs::path(instrument_name + std::string(".bit"));
        const auto bistream_filename = live_instrument_dirname / bistream_basename;
        ctx.logf<INFO>("FpgaManager: Loading bitstream {}...\n", bistream_filename);
        FILE *fbitstream = fopen(bistream_filename.c_str(), "rb");

        if (fbitstream == nullptr) {
            ctx.log<ERROR>("FpgaManager: Cannot open bitstream file\n");
            return -1;
        }

        fseek(fbitstream, 0, SEEK_END);
        bitstream_data.resize(ftell(fbitstream));
        rewind(fbitstream);

        if (fread(bitstream_data.data(), bitstream_data.size(), 1, fbitstream) != 1) {
            ctx.log<ERROR>("FpgaManager: Cannot read bitstream data\n");
            fclose(fbitstream);
            return -1;
        }

        fclose(fbitstream);
        return 0;
    }

    int load_bitstream_xdevcfg() {
        FILE *xdevcfg = fopen(xdev.c_str(), "w");

        if (xdevcfg == nullptr) {
            ctx.log<ERROR>("FpgaManager: Cannot open xdevcfg\n");
            return -1;
        }

        std::vector<char> bitstream_data{};

        if (read_bitstream_data(bitstream_data) < 0) {
            fclose(xdevcfg);
            return -1;
        }

        if (fwrite(bitstream_data.data(), bitstream_data.size(), 1, xdevcfg) != 1) {
            ctx.log<ERROR>("FpgaManager: Failed to write bitstream to xdevcfg\n");
            fclose(xdevcfg);
            return -1;
        }

        fclose(xdevcfg);
        return check_bitstream_loaded(xdev_fpga_done, '1');
    }
}; // FpgaManager

#endif // __SERVER_CONTEXT_FPGA_MANAGER__
