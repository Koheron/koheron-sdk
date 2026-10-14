// Zynq bitstream loader
// (c) Koheron

#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/firmware_files.hpp"
#include "server/hardware/fpga_load.hpp"
#include "server/runtime/syslog.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <charconv>
#include <limits>
#include <utility>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

namespace hw {

namespace fs = std::filesystem;

// Forward decl for helper used by log_mem_wc_mappings()
static std::string find_devnode_for_platform_dev(const fs::path& plat_dev, const FpgaManager::Paths& paths);

static std::unexpected<FpgaLoadError> load_error(std::string_view stage, const fs::path& path,
                                              std::error_code code) {
    return std::unexpected(FpgaLoadError{stage, path, code, std::nullopt});
}

static std::vector<fs::path> directory_paths(const fs::path& path, std::error_code& ec) {
    std::vector<fs::path> entries;
    fs::directory_iterator entry{path, ec}, end;
    if (ec) return entries;
    for (; entry != end; entry.increment(ec)) { entries.push_back(entry->path()); }
    return entries;
}

// Device binding and mapping diagnostics are best-effort, as before. Keep their
// filesystem failures visible without throwing in exception-free builds.
static void auxiliary_error(const fs::path& path, const std::error_code& ec) {
    if (ec) logf<ERROR>("FpgaManager: device setup at '{}' failed: {}\n", path, ec.message());
}

static bool auxiliary_exists(const fs::path& path) {
    std::error_code ec;
    const bool exists = fs::exists(path, ec);
    auxiliary_error(path, ec);
    return exists && !ec;
}

static bool auxiliary_directory(const fs::path& path) {
    std::error_code ec;
    const bool directory = fs::is_directory(path, ec);
    auxiliary_error(path, ec);
    return directory && !ec;
}

static bool auxiliary_symlink(const fs::path& path) {
    std::error_code ec;
    const bool symlink = fs::is_symlink(path, ec);
    auxiliary_error(path, ec);
    return symlink && !ec;
}

static std::vector<fs::path> auxiliary_entries(const fs::path& path) {
    std::error_code ec;
    auto entries = directory_paths(path, ec);
    auxiliary_error(path, ec);
    if (ec) entries.clear();
    return entries;
}

// -----------------------------------------------------------------------------
// Helpers (quiet on benign absence, loud on real failures)
// -----------------------------------------------------------------------------

// Unbind any platform devices claimed by a driver (returns count, quiet if missing)
static int unbind_all(const fs::path& sys, const char* drvdir) {
    const fs::path base = sys / "bus/platform/drivers" / drvdir;
    if (!auxiliary_exists(base)) return 0;             // driver not loaded → benign
    const auto unbind = base / "unbind";
    if (!auxiliary_exists(unbind)) return 0;           // no unbind file → benign
    int count = 0;
    for (const auto& e : auxiliary_entries(base)) {
        if (!auxiliary_symlink(e)) continue; // bound devices appear as symlinks here
        const auto devname = e.filename().string();
        const auto ec = write_fpga_attribute(unbind, devname);
        if (!ec) ++count;
        auxiliary_error(unbind, ec);
    }
    return count;
}

// Enable/disable fpga_region (quiet if path absent)
static void region_enable(const fs::path& path, bool en) {
    if (!auxiliary_exists(path)) return; // platform without fpga_region → benign
    auxiliary_error(path, write_fpga_attribute(path, en ? "1" : "0"));
}

// Create /dev/uio* if udev is absent/asleep (returns created count; logs only on hard failures)
static int synthesize_uio_devnodes(const FpgaManager::Paths& paths) {
    const fs::path cls = paths.sys / "class/uio";
    if (!auxiliary_exists(cls)) return 0;
    int made = 0;
    for (const auto& u : auxiliary_entries(cls)) {
        if (!auxiliary_directory(u)) continue;
        const auto name = u.filename().string(); // uio0, uio1...
        std::ifstream f(u / "dev");
        if (!f.good()) {
            logf<ERROR>("FpgaManager: cannot read {}/dev\n", u);
            continue;
        }
        std::string dev; f >> dev; if (dev.empty()) { continue; }
        auto colon = dev.find(':'); if (colon == std::string::npos) { continue; }
        unsigned maj{}, min{};
        const auto major = std::from_chars(dev.data(), dev.data() + colon, maj);
        const auto minor = std::from_chars(dev.data() + colon + 1, dev.data() + dev.size(), min);
        if (major.ec != std::errc{} || major.ptr != dev.data() + colon ||
            minor.ec != std::errc{} || minor.ptr != dev.data() + dev.size()) {
            logf<ERROR>("FpgaManager: invalid UIO device numbers at {}\n", u / "dev");
            continue;
        }
        const auto node = paths.dev / name;
        if (access(node.c_str(), F_OK) != 0) {
            if (mknod(node.c_str(), S_IFCHR | 0660, makedev(maj, min)) != 0) {
                logf<ERROR>("FpgaManager: mknod({}) failed ({}: {})\n", node, errno, strerror(errno));
            } else {
                ++made;
            }
        }
    }
    return made;
}

// Map platform device -> /dev/uioX (if bound), return matched uio node name (no logs)
static std::string map_to_uio_node(const fs::path& plat_dev_path, const fs::path& sys) {
    const fs::path uio_cls = sys / "class/uio";
    if (!auxiliary_exists(uio_cls)) return {};
    for (const auto& u : auxiliary_entries(uio_cls)) {
        if (!auxiliary_directory(u)) continue;
        const auto devlink = u / "device";
        if (!auxiliary_exists(devlink)) continue;
        std::error_code ec;
        auto canon_link = fs::canonical(devlink, ec); if (ec) continue;
        auto canon_dev  = fs::canonical(plat_dev_path, ec); if (ec) continue;
        if (canon_link == canon_dev) return u.filename().string(); // "uio0"
    }
    return {};
}

// Try to find a /dev node for a platform device by scanning likely classes
static std::string find_devnode_for_platform_dev(const fs::path& plat_dev, const FpgaManager::Paths& paths) {
    const std::vector<fs::path> classes = {
        paths.sys / "class/misc",
        paths.sys / "class/mem_wc",
        paths.sys / "class/char"
    };
    std::error_code ec;
    auto canon_dev = fs::canonical(plat_dev, ec);
    if (ec) return {};

    for (const auto& cls : classes) {
        if (!auxiliary_exists(cls)) continue;
        for (const auto& e : auxiliary_entries(cls)) {
            if (!auxiliary_directory(e)) continue;
            const auto devlink = e / "device";
            if (!auxiliary_exists(devlink)) continue;
            auto canon_link = fs::canonical(devlink, ec);
            if (ec) continue;
            if (canon_link == canon_dev) {
                return (paths.dev / e.filename()).string();
            }
        }
    }
    const auto mem_wc = paths.dev / "mem_wc";
    if (access(mem_wc.c_str(), F_OK) == 0) return mem_wc.string();
    return {};
}

// Bind/ensure all generic-uio devices and print per-device mapping lines. No summary.
static void bind_and_log_uio_mappings(const FpgaManager::Paths& paths) {
    const fs::path drv = paths.sys / "bus/platform/drivers/uio_pdrv_genirq";
    const fs::path plat = paths.sys / "bus/platform/devices";
    if (!auxiliary_exists(drv) || !auxiliary_exists(plat)) return;

    // Deterministic order
    auto devs = auxiliary_entries(plat);
    std::sort(devs.begin(), devs.end(),
              [](const fs::path& a, const fs::path& b){ return a.filename().string() < b.filename().string(); });

    // First pass: bind if needed
    for (const auto& devdir : devs) {
        const auto compat = devdir / "of_node/compatible";
        if (!auxiliary_exists(compat)) continue;
        std::ifstream f(compat, std::ios::binary);
        if (!f.good()) continue;
        std::string c((std::istreambuf_iterator<char>(f)), {});
        if (c.find("generic-uio") == std::string::npos) continue;

        bool is_bound = false;
        if (auxiliary_exists(devdir / "driver")) {
            std::error_code ec;
            auto drvlink = fs::canonical(devdir / "driver", ec);
            if (!ec && drvlink.filename().string() == "uio_pdrv_genirq") is_bound = true;
        }
        if (!is_bound) {
            const auto devname = devdir.filename().string();
            auxiliary_error(drv / "bind", write_fpga_attribute(drv / "bind", devname));
        }
    }

    // Ensure /dev/uio* exist, then print per-device mapping
    (void)synthesize_uio_devnodes(paths);

    for (const auto& devdir : devs) {
        const auto compat = devdir / "of_node/compatible";
        if (!auxiliary_exists(compat)) continue;
        std::ifstream f(compat, std::ios::binary);
        if (!f.good()) continue;
        std::string c((std::istreambuf_iterator<char>(f)), {});
        if (c.find("generic-uio") == std::string::npos) continue;

        const auto devname  = devdir.filename().string();
        const auto uio_node = map_to_uio_node(devdir, paths.sys);
        if (!uio_node.empty()) {
            logf<INFO>("FpgaManager: '{}' mapped to /dev/{}\n", devname, uio_node);
        } else {
            logf<ERROR>("FpgaManager: '{}' has no /dev/uio mapping yet\n", devname);
        }
    }
}

// Enumerate devices bound to platform driver 'mem_wc' and print mapping lines (only if present).
static void log_mem_wc_mappings(const FpgaManager::Paths& paths) {
    const fs::path drv = paths.sys / "bus/platform/drivers/mem_wc";
    if (!auxiliary_exists(drv)) return; // stay quiet if driver not present

    for (const auto& e : auxiliary_entries(drv)) {
        if (!auxiliary_symlink(e)) continue; // bound devices are symlinks
        const auto devname = e.filename().string();
        const auto devpath = paths.sys / "bus/platform/devices" / devname;
        if (!auxiliary_exists(devpath)) continue;

        const auto node = find_devnode_for_platform_dev(devpath, paths);
        if (!node.empty()) {
            logf<INFO>("FpgaManager: '{}' mapped to {}\n", devname, node);
        }
    }
}

// -----------------------------------------------------------------------------
// FpgaManager
// -----------------------------------------------------------------------------

FpgaManager::FpgaManager() : FpgaManager(Paths{}) {}

FpgaManager::FpgaManager(Paths selected_paths)
: paths(std::move(selected_paths))
, live_instrument_dirname(paths.live)
, fmanager_flags(paths.sys / "class/fpga_manager/fpga0/flags")
, fmanager_firmware(paths.sys / "class/fpga_manager/fpga0/firmware")
, fmanager_state(paths.sys / "class/fpga_manager/fpga0/state")
, overlay_path(paths.sys / "kernel/config/device-tree/overlays/full")
, overlay_fpga_done(overlay_path / "status")
, configfs_root(paths.sys / "kernel/config")
, configfs_dt(configfs_root / "device-tree")
, overlays_root(configfs_dt / "overlays")
, region_enable_path(paths.sys / "class/fpga_region/region0/enable")
, xdev(paths.dev / "xdevcfg")
, xdev_fpga_done(paths.sys / "bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done")
{}

int FpgaManager::load_bitstream() {
    const auto result = load_bitstream_checked();
    if (result) return 0;
    const auto& error = result.error();
    logf<ERROR>("FpgaManager: {} at '{}' failed: {}\n", error.stage, error.path, error.code.message());
    if (error.cleanup) {
        logf<ERROR>("FpgaManager: removing failed overlay '{}' also failed: {}\n",
                    error.cleanup->path, error.cleanup->code.message());
    }
    errno = error.code.category() == std::generic_category() || error.code.category() == std::system_category()
        ? error.code.value() : EIO;
    return -1;
}

FpgaManager::Result FpgaManager::load_bitstream_checked() {
    std::error_code ec;
    const bool legacy = fs::exists(xdev, ec);
    if (ec) return load_error("select backend", xdev, ec);
    if (legacy) return load_bitstream_xdevcfg();
    const bool overlay = fs::exists(fmanager_flags, ec);
    if (ec) return load_error("select backend", fmanager_flags, ec);
    if (overlay) return load_bitstream_overlay();
    return load_error("select backend", fmanager_flags, std::make_error_code(std::errc::no_such_device));
}

FpgaManager::Result FpgaManager::check_bitstream_loaded(const Path& path, char expected) {
    const auto result = check_fpga_attribute(path, std::string(1, expected));
    if (!result) return load_error("verify xdevcfg programming", path, result.error());
    log("FpgaManager: Bitstream successfully loaded\n");
    return {};
}

// -----------------------------------------------------------------------------
// Overlay path
// -----------------------------------------------------------------------------

FpgaManager::Result FpgaManager::prepare_firmware() {
    const auto bitbin = Path{INSTRUMENT_NAME ".bit.bin"};
    const auto result = prepare_firmware_files(live_instrument_dirname, bitbin, paths.firmware,
                                              paths.sys / "module/firmware_class/parameters/path");
    if (result.error) return load_error("prepare firmware", result.failed_path, result.error);
    return {};
}

FpgaManager::Result FpgaManager::clean_up_previous_overlays() {
    std::error_code ec;
    const bool exists = fs::exists(overlays_root, ec);
    if (ec) return load_error("inspect old overlays", overlays_root, ec);
    if (!exists) return {};
    const auto entries = directory_paths(overlays_root, ec);
    if (ec) return load_error("inspect old overlays", overlays_root, ec);
    if (entries.empty()) return {};

    std::vector<Path> dirs;
    for (const auto& entry : entries) {
        const bool directory = fs::is_directory(entry, ec);
        if (ec) return load_error("inspect old overlays", entry, ec);
        if (directory) dirs.push_back(entry);
    }
    std::sort(dirs.rbegin(), dirs.rend());

    (void)unbind_all(paths.sys, "uio_pdrv_genirq");
    region_enable(region_enable_path, false);
    const auto result = [&]() -> Result {
        for (int pass = 0; pass < 5; ++pass) {
            Path failed_path = overlays_root;
            auto removal_error = std::make_error_code(std::errc::device_or_resource_busy);
            for (const auto& dir : dirs) {
                const bool present = fs::exists(dir, ec);
                if (ec) return load_error("remove old overlay", dir, ec);
                if (!present) continue;
                if (::rmdir(dir.c_str()) < 0 && errno != ENOENT) {
                    failed_path = dir;
                    removal_error = {errno, std::generic_category()};
                }
            }
            const auto remaining = directory_paths(overlays_root, ec);
            if (ec) return load_error("inspect old overlays", overlays_root, ec);
            if (remaining.empty()) return {};
            if (pass == 4) return load_error("remove old overlay", failed_path, removal_error);
            usleep(100 * 1000);
        }
        return {};
    }();
    region_enable(region_enable_path, true);
    return result;
}

FpgaManager::Result FpgaManager::mount_configfs() {
    std::error_code ec;
    const bool root_exists = fs::exists(configfs_root, ec);
    if (ec) return load_error("prepare configfs", configfs_root, ec);
    if (!root_exists) {
        fs::create_directories(configfs_root, ec);
        if (ec) return load_error("prepare configfs", configfs_root, ec);
    }
    const bool mounted = fs::exists(configfs_dt, ec);
    if (ec) return load_error("mount configfs", configfs_dt, ec);
    if (!mounted && mount("none", configfs_root.c_str(), "configfs", 0, nullptr) < 0) {
        return load_error("mount configfs", configfs_root, {errno, std::generic_category()});
    }
    return {};
}

FpgaManager::Result FpgaManager::setup_overlay_path() {
    return mount_configfs()
        .and_then([this] { return clean_up_previous_overlays(); })
        .and_then([this]() -> Result {
            std::error_code ec;
            fs::create_directories(overlay_path, ec);
            if (ec) return load_error("create overlay", overlay_path, ec);
            return {};
        });
}

FpgaManager::Result FpgaManager::setup_fmanager_flags() {
    const auto ec = write_fpga_attribute(fmanager_flags, "0\n");
    if (ec) return load_error("set FPGA flags", fmanager_flags, ec);
    return {};
}

FpgaManager::Result FpgaManager::program_bitstream() {
    const std::string firmware = INSTRUMENT_NAME ".bit.bin\n";
    if (firmware.size() >= 255) {
        return load_error("program bitstream", fmanager_firmware, std::make_error_code(std::errc::filename_too_long));
    }
    const auto ec = write_fpga_attribute(fmanager_firmware, firmware);
    if (ec) return load_error("program bitstream", fmanager_firmware, ec);
    const auto state = check_fpga_attribute(fmanager_state, "operating");
    if (!state) return load_error("verify programming", fmanager_state, state.error());
    return {};
}

FpgaManager::Result FpgaManager::write_overlay() {
    const auto path = overlay_path / "path";
    const auto ec = write_fpga_attribute(path, "pl.dtbo\n");
    if (ec) return load_error("apply overlay", path, ec);
    return {};
}

FpgaManager::Result FpgaManager::discard_overlay(Result result) {
    if (result) return result;
    if (::rmdir(overlay_path.c_str()) < 0 && errno != ENOENT) {
        result.error().cleanup = FpgaCleanupError{overlay_path, {errno, std::generic_category()}};
    }
    return result;
}

FpgaManager::Result FpgaManager::load_bitstream_overlay() {
    logf<INFO>("FpgaManager: Loading {}\n", Path{INSTRUMENT_NAME ".bit.bin"});
    if (const auto result = prepare_firmware(); !result) return result;

    std::error_code ec;
    const auto bridges = paths.sys / "class/fpga_bridge";
    const bool bridges_exist = fs::exists(bridges, ec);
    if (ec) return load_error("check FPGA bridges", bridges, ec);
    if (bridges_exist) {
        const bool empty = fs::is_empty(bridges, ec);
        if (ec) return load_error("check FPGA bridges", bridges, ec);
        if (!empty) return load_error("check FPGA bridges", bridges, std::make_error_code(std::errc::operation_not_supported));
    }
    const bool firmware_exists = fs::exists(fmanager_firmware, ec);
    if (ec) return load_error("check firmware interface", fmanager_firmware, ec);
    if (!firmware_exists) {
        return load_error("check firmware interface", fmanager_firmware, std::make_error_code(std::errc::no_such_file_or_directory));
    }
    if (const auto result = setup_overlay_path(); !result) return result;

    auto result = setup_fmanager_flags()
        .and_then([this] { return program_bitstream(); })
        .and_then([this] { return write_overlay(); })
        .and_then([this]() -> Result {
            const auto path = overlay_path / "path";
            const auto selected = check_fpga_attribute(path, "pl.dtbo");
            if (!selected) return load_error("verify overlay path", path, selected.error());
            const auto applied = check_fpga_attribute(overlay_fpga_done, "applied");
            if (!applied) return load_error("verify overlay status", overlay_fpga_done, applied.error());
            return {};
        });
    if (!result) return discard_overlay(std::move(result));
    log("FpgaManager: Bitstream and device overlay successfully loaded\n");
    bind_and_log_uio_mappings(paths);
    log_mem_wc_mappings(paths);
    return {};
}

// -----------------------------------------------------------------------------
// xdevcfg flow (legacy)
// -----------------------------------------------------------------------------

FpgaManager::Result FpgaManager::read_bitstream_data(std::vector<char>& bitstream_data) {
    const auto filename = live_instrument_dirname / INSTRUMENT_NAME ".bit";
    std::error_code ec;
    const auto size = fs::file_size(filename, ec);
    if (ec) return load_error("read bitstream", filename, ec);
    if (size == 0) return load_error("read bitstream", filename, std::make_error_code(std::errc::invalid_argument));
    if (size > bitstream_data.max_size() || size > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return load_error("read bitstream", filename, std::make_error_code(std::errc::value_too_large));
    }
    std::ifstream input(filename, std::ios::binary);
    if (!input) return load_error("read bitstream", filename, std::make_error_code(std::io_errc::stream));
    bitstream_data.resize(static_cast<size_t>(size));
    input.read(bitstream_data.data(), static_cast<std::streamsize>(size));
    input.close();
    if (!input) return load_error("read bitstream", filename, std::make_error_code(std::io_errc::stream));
    return {};
}

FpgaManager::Result FpgaManager::load_bitstream_xdevcfg() {
    std::vector<char> data;
    if (const auto result = read_bitstream_data(data); !result) return result;
    const auto ec = write_fpga_attribute(xdev, std::string_view(data.data(), data.size()));
    if (ec) return load_error("program xdevcfg", xdev, ec);
    return check_bitstream_loaded(xdev_fpga_done, '1');
}

} // namespace hw
