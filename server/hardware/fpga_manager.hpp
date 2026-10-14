// Zynq bitstream loader
// (c) Koheron

#ifndef __SERVER_CONTEXT_FPGA_MANAGER__
#define __SERVER_CONTEXT_FPGA_MANAGER__

#include <filesystem>
#include <vector>
#include <expected>
#include <optional>
#include <string_view>
#include <system_error>

namespace hw {

struct FpgaCleanupError {
    std::filesystem::path path;
    std::error_code code;
};

struct FpgaLoadError {
    std::string_view stage;
    std::filesystem::path path;
    std::error_code code;
    std::optional<FpgaCleanupError> cleanup;
};

class FpgaManager {
    using Path = std::filesystem::path;

  public:
    using Result = std::expected<void, FpgaLoadError>;
    struct Paths {
        Path live = "/tmp/live-instrument";
        Path sys = "/sys";
        Path dev = "/dev";
        Path firmware = "/lib/firmware";
    };

    FpgaManager();
    explicit FpgaManager(Paths paths);
    int load_bitstream();
    [[nodiscard]] Result load_bitstream_checked();

  private:
    const Paths paths;

    const Path live_instrument_dirname;

    Result check_bitstream_loaded(const Path& fprog_done_path, char expected);

    // ------------------------------------------------------------------------
    // Load using overlay (fpga_manager)
    // ------------------------------------------------------------------------
    const Path fmanager_flags;
    const Path fmanager_firmware;
    const Path fmanager_state;
    const Path overlay_path;
    const Path overlay_fpga_done;
    const Path configfs_root;
    const Path configfs_dt;
    const Path overlays_root;
    const Path region_enable_path;

    Result prepare_firmware();
    Result clean_up_previous_overlays();
    Result mount_configfs();
    Result setup_overlay_path();
    Result setup_fmanager_flags();
    Result program_bitstream();
    Result write_overlay();
    Result load_bitstream_overlay();
    Result discard_overlay(Result result);

    // ------------------------------------------------------------------------
    // Load using xdevcfg
    // ------------------------------------------------------------------------
    const Path xdev;
    const Path xdev_fpga_done;

    Result read_bitstream_data(std::vector<char>& bitstream_data);
    Result load_bitstream_xdevcfg();
}; // FpgaManager

} // namespace hw

#endif // __SERVER_CONTEXT_FPGA_MANAGER__
