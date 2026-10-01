// Zynq bitstream loader
// (c) Koheron

#ifndef __SERVER_CONTEXT_FPGA_MANAGER__
#define __SERVER_CONTEXT_FPGA_MANAGER__

#include <filesystem>
#include <vector>

namespace hw {

class FpgaManager {
    using Path = std::filesystem::path;

  public:
    FpgaManager();
    int load_bitstream();

  private:
    bool use_xdevcgf = false;   // kept spelling to match existing codebase
    bool use_overlay = false;

    const Path live_instrument_dirname = "/tmp/live-instrument/";

    int check_bitstream_loaded(const Path& fprog_done_path, char expected);

    // ------------------------------------------------------------------------
    // Load using overlay (fpga_manager)
    // ------------------------------------------------------------------------
    const Path fmanager_flags    = "/sys/class/fpga_manager/fpga0/flags";
    const Path overlay_path      = "/sys/kernel/config/device-tree/overlays/full";
    const Path overlay_fpga_done = "/sys/kernel/config/device-tree/overlays/full/status";

    int copy_firmware();
    int clean_up_previous_overlays();
    int mount_configfs();
    int setup_overlay_path();
    int setup_fmanager_flags();
    int write_overlay();
    int load_bitstream_overlay();

    // ------------------------------------------------------------------------
    // Load using xdevcfg
    // ------------------------------------------------------------------------
    const Path xdev           = "/dev/xdevcfg";
    const Path xdev_fpga_done = "/sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done";

    int read_bitstream_data(std::vector<char>& bitstream_data);
    int load_bitstream_xdevcfg();
}; // FpgaManager

} // namespace hw

#endif // __SERVER_CONTEXT_FPGA_MANAGER__
