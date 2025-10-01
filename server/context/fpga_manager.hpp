// Zynq bistream loader
// (c) Koheron

#ifndef __SERVER_CONTEXT_FPGA_MANAGER__
#define __SERVER_CONTEXT_FPGA_MANAGER__

#include <string>
#include <filesystem>
#include <vector>

#define str_(s) #s
#define xstr_(s) str_(s)

namespace {
    namespace fs = std::filesystem;
}

class FpgaManager {
  public:
    FpgaManager();
    int load_bitstream();

  private:
    const std::string instrument_name = xstr_(INSTRUMENT_NAME);

    bool use_xdevcgf = false;
    bool use_overlay = false;

    const fs::path live_instrument_dirname = "/tmp/live-instrument/";

    int check_bitstream_loaded(const fs::path& fprog_done_path, char expected);

    // ------------------------------------------------------------------------
    // Load using overlay
    // ------------------------------------------------------------------------

    const fs::path fmanager_flags = "/sys/class/fpga_manager/fpga0/flags";
    const fs::path overlay_path = "/configfs/device-tree/overlays/full";
    const fs::path overlay_fpga_done = "/configfs/device-tree/overlays/full/status";

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

    const fs::path xdev = "/dev/xdevcfg";
    const fs::path xdev_fpga_done = "/sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done";

    int read_bitstream_data(std::vector<char>& bitstream_data);
    int load_bitstream_xdevcfg();
}; // FpgaManager

#endif // __SERVER_CONTEXT_FPGA_MANAGER__
