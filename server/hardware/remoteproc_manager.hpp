// Cortex-R5 remoteproc controller
// (c) Koheron

#ifndef __SERVER_HARDWARE_REMOTEPROC_MANAGER__
#define __SERVER_HARDWARE_REMOTEPROC_MANAGER__

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace hw {

class RpuFirmwareManager {
    using Path = std::filesystem::path;

  public:
    RpuFirmwareManager() = default;

    int install_all();
    int start_all();
    int stop_all();
    int start(std::string_view remoteproc_id);
    int stop(std::string_view remoteproc_id);
    std::vector<std::string> known_remoteprocs();

  private:
    struct RemoteProcBinding {
        std::string remoteproc;
        std::string firmware;
    };

    const Path live_instrument_dirname{"/tmp/live-instrument/"};
    const Path firmware_manifest_{live_instrument_dirname / "firmware.manifest"};
    const Path remoteproc_manifest_{live_instrument_dirname / "remoteproc.manifest"};
    const Path firmware_root_{"/lib/firmware"};
    const Path remoteproc_root_{"/sys/class/remoteproc"};

    int install_manifest_entries();
    int refresh_bindings();
    int set_remoteproc_firmware(const RemoteProcBinding& binding);
    int start_binding(const RemoteProcBinding& binding);
    int stop_binding(const std::string& remoteproc_id);

    static std::string trim(std::string value);
    static bool write_sysfs(const Path& path, std::string_view value);
    static std::string read_file_trimmed(const Path& path);
    bool wait_for_state(const Path& state_path, std::string_view desired,
                        std::string_view context) const;

    std::vector<RemoteProcBinding> bindings_;
    bool bindings_loaded_ = false;
};

} // namespace hw

#endif // __SERVER_HARDWARE_REMOTEPROC_MANAGER__
