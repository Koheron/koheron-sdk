
#include "server/context/fpga_manager.hpp"
#include "server/runtime/syslog.hpp"

#include <array>
#include <sys/mount.h>
#include <unistd.h>

namespace fs = std::filesystem;

FpgaManager::FpgaManager() {
    if (fs::exists(xdev)) {
        koheron::print<INFO>("FpgaManager: Bitstream loading method: xdevcfg\n");
        use_xdevcgf = true;
        return;
    }

    if (fs::exists(fmanager_flags)) {
        koheron::print<INFO>("FpgaManager: Bitstream loading method: fpga_manager\n");
        use_overlay = true;
        return;
    }

    koheron::print<PANIC>("FpgaManager: Failed to identify bitstream loading mechanism\n");
    exit(EXIT_FAILURE);
}

int FpgaManager::load_bitstream() {
    if (use_xdevcgf) {
        return load_bitstream_xdevcfg();
    }

    if (use_overlay) {
        return load_bitstream_overlay();
    }

    return -1;
}

int FpgaManager::check_bitstream_loaded(const fs::path& fprog_done_path, char expected) {
    FILE *fprog_done = fopen(fprog_done_path.c_str(), "r");

    if (fprog_done == nullptr) {
        koheron::print<PANIC>("FpgaManager: Failed to open FPGA status\n");
        return -1;
    }

    std::array<char, 1> buff{};

    if (read(fileno(fprog_done), buff.data(), 1) == 1) {
        if (buff[0] == expected) {
            koheron::print<INFO>("FpgaManager: Bitstream successfully loaded\n");
            fclose(fprog_done);
            return 0;
        } else {
            koheron::print<PANIC>("FpgaManager: Failed to load bitstream\n");
            fclose(fprog_done);
            return -1;
        }
    } else {
        koheron::print<PANIC>("FpgaManager: Failed to read FPGA status\n");
        fclose(fprog_done);
        return -1;
    }
}

// ------------------------------------------------------------------------
// Load using overlay
// ------------------------------------------------------------------------

int FpgaManager::copy_firmware() {
    const fs::path lib_firmware_dirname = "/lib/firmware/";
    const auto bistream_name = fs::path{INSTRUMENT_NAME ".bit.bin"};
    koheron::print_fmt<INFO>("FpgaManager: Loading bitstream {}...\n", bistream_name);

    if (!fs::exists(lib_firmware_dirname)) {
        fs::create_directories(lib_firmware_dirname);
    }

    if (!fs::copy_file(live_instrument_dirname / "pl.dtbo",
                       lib_firmware_dirname / "pl.dtbo",
                       fs::copy_options::overwrite_existing)) {
        koheron::print<ERROR>("FpgaManager: pl.dtbo copy failed\n");
        return -1;
    }

    if (!fs::copy_file(live_instrument_dirname / bistream_name,
                       lib_firmware_dirname / bistream_name,
                       fs::copy_options::overwrite_existing)) {
        koheron::print<ERROR>("FpgaManager: bistream copy failed\n");
        return -1;
    }

    return 0;
}

int FpgaManager::clean_up_previous_overlays() {
    if (fs::exists(overlay_path)) {
        // In the device-tree overlay configfs, you’re supposed to remove an overlay by rmdir on the overlay directory.
        // You’re not allowed to unlink the files inside (like dtbo). Kernel refuses that with EPERM (“Operation not permitted”).
        // std::filesystem::remove_all tries to recursively unlink everything inside before removing the directory, so it hits the dtbo file and throws filesystem_error.
        // So we cannot use fs::remove_all(overlay_path) here.
        ::rmdir(overlay_path.c_str());
    }

    if (fs::exists(overlay_path)) {
        koheron::print<PANIC>("FpgaManager: Failed to remove previous overlay\n");
        return -1;
    }

    return 0;
}

int FpgaManager::mount_configfs() {
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

int FpgaManager::setup_overlay_path() {
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

int FpgaManager::setup_fmanager_flags() {
    // Set flag to flash entire firmware (i.e. not partial reconfig)
    FILE *xflag = fopen(fmanager_flags.c_str(), "w");

    if (xflag == nullptr) {
        koheron::print<ERROR>("FpgaManager: Cannot open xflag\n");
        return -1;
    }

    if (fwrite("0", 1, 1, xflag) != 1) {
        fclose(xflag);
        return -1;
    }

    fclose(xflag);
    return 0;
}

int FpgaManager::write_overlay() {
    const fs::path overlay = overlay_path / "path";
    FILE *overlay_file = fopen(overlay.c_str(), "w");

    if (overlay_file == nullptr) {
        koheron::print<ERROR>("FpgaManager: Cannot open overlay\n");
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

int FpgaManager::load_bitstream_overlay() {
    if (copy_firmware() < 0) {
        koheron::print<ERROR>("FpgaManager: Failed to copy firmware\n");
        return -1;
    }

    if (setup_overlay_path() < 0) {
        koheron::print<ERROR>("FpgaManager: Failed to setup overlay path\n");
        return -1;
    }

    if (setup_fmanager_flags() < 0) {
        koheron::print_fmt<ERROR>("FpgaManager: Failed to set flag on {}\n", fmanager_flags);
        return -1;
    }

    if (write_overlay() < 0) {
        koheron::print_fmt<ERROR>("FpgaManager: Failed to write bitstream to {}\n", overlay_path);
        return -1;
    }

    return check_bitstream_loaded(overlay_fpga_done, 'a');
}

// ------------------------------------------------------------------------
// Load using xdevcfg
// ------------------------------------------------------------------------

int FpgaManager::read_bitstream_data(std::vector<char>& bitstream_data) {
    // https://stackoverflow.com/questions/22059189/read-a-file-as-byte-array
    const auto bistream_basename = fs::path{INSTRUMENT_NAME ".bit"};
    const auto bistream_filename = live_instrument_dirname / bistream_basename;
    koheron::print_fmt<INFO>("FpgaManager: Loading bitstream {}...\n", bistream_filename);
    FILE *fbitstream = fopen(bistream_filename.c_str(), "rb");

    if (fbitstream == nullptr) {
        koheron::print_fmt<ERROR>("FpgaManager: Cannot open bitstream file\n");
        return -1;
    }

    fseek(fbitstream, 0, SEEK_END);
    bitstream_data.resize(ftell(fbitstream));
    rewind(fbitstream);

    if (fread(bitstream_data.data(), bitstream_data.size(), 1, fbitstream) != 1) {
        koheron::print_fmt<ERROR>("FpgaManager: Cannot read bitstream data\n");
        fclose(fbitstream);
        return -1;
    }

    fclose(fbitstream);
    return 0;
}

int FpgaManager::load_bitstream_xdevcfg() {
    FILE *xdevcfg = fopen(xdev.c_str(), "w");

    if (xdevcfg == nullptr) {
        koheron::print_fmt<ERROR>("FpgaManager: Cannot open xdevcfg\n");
        return -1;
    }

    std::vector<char> bitstream_data{};

    if (read_bitstream_data(bitstream_data) < 0) {
        fclose(xdevcfg);
        return -1;
    }

    if (fwrite(bitstream_data.data(), bitstream_data.size(), 1, xdevcfg) != 1) {
        koheron::print_fmt<ERROR>("FpgaManager: Failed to write bitstream to xdevcfg\n");
        fclose(xdevcfg);
        return -1;
    }

    fclose(xdevcfg);
    return check_bitstream_loaded(xdev_fpga_done, '1');
}