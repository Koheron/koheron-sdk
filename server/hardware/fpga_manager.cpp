// Zynq bitstream loader
// (c) Koheron

#include "server/hardware/fpga_manager.hpp"
#include "server/runtime/syslog.hpp"

#include <array>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

namespace hw {

namespace fs = std::filesystem;

// Forward decl for helper used by log_mem_wc_mappings()
static std::string find_devnode_for_platform_dev(const fs::path& plat_dev);

// Canonical configfs paths (do not mix with /configfs)
static const fs::path kCfgfsRoot{"/sys/kernel/config"};
static const fs::path kCfgfsDT  {"/sys/kernel/config/device-tree"};
static const fs::path kOverlaysRoot = kCfgfsDT / "overlays";
static const fs::path kRegionEnable{"/sys/class/fpga_region/region0/enable"};

// -----------------------------------------------------------------------------
// Helpers (quiet on benign absence, loud on real failures)
// -----------------------------------------------------------------------------

// Unbind any platform devices claimed by a driver (returns count, quiet if missing)
static int unbind_all(const char* drvdir) {
    const fs::path base{std::string("/sys/bus/platform/drivers/") + drvdir};
    if (!fs::exists(base)) return 0;             // driver not loaded → benign
    const auto unbind = base / "unbind";
    if (!fs::exists(unbind)) return 0;           // no unbind file → benign
    int count = 0;
    for (const auto& e : fs::directory_iterator(base)) {
        if (!fs::is_symlink(e.path())) continue; // bound devices appear as symlinks here
        const auto devname = e.path().filename().string();
        if (FILE* f = fopen(unbind.c_str(), "w")) {
            if (fwrite(devname.c_str(), devname.size(), 1, f) == 1) ++count;
            fclose(f);
        }
    }
    return count;
}

// Enable/disable fpga_region (quiet if path absent)
static void region_enable(bool en) {
    if (!fs::exists(kRegionEnable)) return; // platform without fpga_region → benign
    if (FILE* f = fopen(kRegionEnable.c_str(), "w")) {
        const char c = en ? '1' : '0';
        (void)fwrite(&c, 1, 1, f);
        fclose(f);
    }
}

// Create /dev/uio* if udev is absent/asleep (returns created count; logs only on hard failures)
static int synthesize_uio_devnodes() {
    const fs::path cls{"/sys/class/uio"};
    if (!fs::exists(cls)) return 0;
    int made = 0;
    for (const auto& u : fs::directory_iterator(cls)) {
        if (!fs::is_directory(u)) continue;
        const auto name = u.path().filename().string(); // uio0, uio1...
        std::ifstream f(u.path() / "dev");
        if (!f.good()) {
            logf<ERROR>("FpgaManager: cannot read {}/dev\n", u.path());
            continue;
        }
        std::string dev; f >> dev; if (dev.empty()) { continue; }
        auto colon = dev.find(':'); if (colon == std::string::npos) { continue; }
        int maj = std::stoi(dev.substr(0, colon));
        int min = std::stoi(dev.substr(colon + 1));
        const auto node = std::string("/dev/") + name;
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
static std::string map_to_uio_node(const fs::path& plat_dev_path) {
    const fs::path uio_cls{"/sys/class/uio"};
    if (!fs::exists(uio_cls)) return {};
    for (const auto& u : fs::directory_iterator(uio_cls)) {
        if (!fs::is_directory(u)) continue;
        const auto devlink = u.path() / "device";
        if (!fs::exists(devlink)) continue;
        std::error_code ec;
        auto canon_link = fs::canonical(devlink, ec); if (ec) continue;
        auto canon_dev  = fs::canonical(plat_dev_path, ec); if (ec) continue;
        if (canon_link == canon_dev) return u.path().filename().string(); // "uio0"
    }
    return {};
}

// Try to find a /dev node for a platform device by scanning likely classes
static std::string find_devnode_for_platform_dev(const fs::path& plat_dev) {
    const std::vector<fs::path> classes = {
        "/sys/class/misc",
        "/sys/class/mem_wc",
        "/sys/class/char"
    };
    std::error_code ec;
    auto canon_dev = fs::canonical(plat_dev, ec);
    if (ec) return {};

    for (const auto& cls : classes) {
        if (!fs::exists(cls)) continue;
        for (const auto& e : fs::directory_iterator(cls)) {
            if (!fs::is_directory(e)) continue;
            const auto devlink = e.path() / "device";
            if (!fs::exists(devlink)) continue;
            auto canon_link = fs::canonical(devlink, ec);
            if (ec) continue;
            if (canon_link == canon_dev) {
                return std::string("/dev/") + e.path().filename().string();
            }
        }
    }
    if (access("/dev/mem_wc", F_OK) == 0) return "/dev/mem_wc";
    return {};
}

// Bind/ensure all generic-uio devices and print per-device mapping lines. No summary.
static void bind_and_log_uio_mappings() {
    const fs::path drv {"/sys/bus/platform/drivers/uio_pdrv_genirq"};
    const fs::path plat{"/sys/bus/platform/devices"};
    if (!fs::exists(drv) || !fs::exists(plat)) return;

    // Deterministic order
    std::vector<fs::path> devs;
    for (const auto& d : fs::directory_iterator(plat)) devs.push_back(d.path());
    std::sort(devs.begin(), devs.end(),
              [](const fs::path& a, const fs::path& b){ return a.filename().string() < b.filename().string(); });

    // First pass: bind if needed
    for (const auto& devdir : devs) {
        const auto compat = devdir / "of_node/compatible";
        if (!fs::exists(compat)) continue;
        std::ifstream f(compat, std::ios::binary);
        if (!f.good()) continue;
        std::string c((std::istreambuf_iterator<char>(f)), {});
        if (c.find("generic-uio") == std::string::npos) continue;

        bool is_bound = false;
        if (fs::exists(devdir / "driver")) {
            std::error_code ec;
            auto drvlink = fs::canonical(devdir / "driver", ec);
            if (!ec && drvlink.filename().string() == "uio_pdrv_genirq") is_bound = true;
        }
        if (!is_bound) {
            if (FILE* b = fopen((drv / "bind").c_str(), "w")) {
                const auto devname = devdir.filename().string();
                (void)fwrite(devname.c_str(), devname.size(), 1, b);
                fclose(b);
            }
        }
    }

    // Ensure /dev/uio* exist, then print per-device mapping
    (void)synthesize_uio_devnodes();

    for (const auto& devdir : devs) {
        const auto compat = devdir / "of_node/compatible";
        if (!fs::exists(compat)) continue;
        std::ifstream f(compat, std::ios::binary);
        if (!f.good()) continue;
        std::string c((std::istreambuf_iterator<char>(f)), {});
        if (c.find("generic-uio") == std::string::npos) continue;

        const auto devname  = devdir.filename().string();
        const auto uio_node = map_to_uio_node(devdir);
        if (!uio_node.empty()) {
            logf<INFO>("FpgaManager: '{}' mapped to /dev/{}\n", devname, uio_node);
        } else {
            logf<ERROR>("FpgaManager: '{}' has no /dev/uio mapping yet\n", devname);
        }
    }
}

// Enumerate devices bound to platform driver 'mem_wc' and print mapping lines (only if present).
static void log_mem_wc_mappings() {
    const fs::path drv{"/sys/bus/platform/drivers/mem_wc"};
    if (!fs::exists(drv)) return; // stay quiet if driver not present

    for (const auto& e : fs::directory_iterator(drv)) {
        if (!fs::is_symlink(e.path())) continue; // bound devices are symlinks
        const auto devname = e.path().filename().string();
        const auto devpath = fs::path("/sys/bus/platform/devices") / devname;
        if (!fs::exists(devpath)) continue;

        const auto node = find_devnode_for_platform_dev(devpath);
        if (!node.empty()) {
            logf<INFO>("FpgaManager: '{}' mapped to {}\n", devname, node);
        }
    }
}

// -----------------------------------------------------------------------------
// FpgaManager
// -----------------------------------------------------------------------------

FpgaManager::FpgaManager() {
    // Pick method silently; the load function will print user-facing lines.
    if (fs::exists(xdev))            { use_xdevcgf = true;  return; }
    if (fs::exists(fmanager_flags))  { use_overlay = true;  return; }
    log<PANIC>("FpgaManager: cannot identify bitstream loading mechanism\n");
    ::_exit(EXIT_FAILURE);
}

int FpgaManager::load_bitstream() {
    if (use_xdevcgf) return load_bitstream_xdevcfg();
    if (use_overlay) return load_bitstream_overlay();
    log<ERROR>("FpgaManager: load_bitstream(): neither xdevcfg nor fpga_manager selected\n");
    return -1;
}

int FpgaManager::check_bitstream_loaded(const fs::path& fprog_done_path, char expected) {
    FILE *f = fopen(fprog_done_path.c_str(), "r");
    if (!f) {
        logf<ERROR>("FpgaManager: check_bitstream_loaded: open('{}') failed ({}: {})\n",
                                  fprog_done_path, errno, strerror(errno));
        log<PANIC>("FpgaManager: open FPGA status failed\n");
        return -1;
    }
    std::array<char,1> b{};
    int r = read(fileno(f), b.data(), 1);
    if (r != 1) {
        logf<ERROR>("FpgaManager: check_bitstream_loaded: read('{}') returned {}\n", fprog_done_path, r);
        fclose(f);
        log<PANIC>("FpgaManager: Failed to read FPGA status\n");
        return -1;
    }
    fclose(f);
    if (b[0] == expected) {
        log("FpgaManager: Bitstream successfully loaded\n");
        return 0;
    }
    logf<ERROR>("FpgaManager: check_bitstream_loaded: got '{}', expected '{}'\n", b[0], expected);
    log<PANIC>("FpgaManager: PL configuration failed\n");
    return -1;
}

// -----------------------------------------------------------------------------
// Overlay path
// -----------------------------------------------------------------------------

int FpgaManager::copy_firmware() {
    const fs::path libfw = "/lib/firmware/";
    const auto bitbin = fs::path{INSTRUMENT_NAME ".bit.bin"};

    if (!fs::exists(live_instrument_dirname)) {
        logf<ERROR>("FpgaManager: copy_firmware: source dir '{}' missing\n", live_instrument_dirname);
        return -1;
    }
    if (!fs::exists(libfw)) {
        std::error_code ec;
        fs::create_directories(libfw, ec);
        if (ec) {
            logf<ERROR>("FpgaManager: copy_firmware: mkdir '{}' failed: {}\n", libfw, ec.message());
            return -1;
        }
    }

    std::error_code ec1, ec2;
    fs::copy_file(live_instrument_dirname / "pl.dtbo", libfw / "pl.dtbo",
                  fs::copy_options::overwrite_existing, ec1);
    if (ec1) {
        logf<ERROR>("FpgaManager: copy_firmware: pl.dtbo copy failed: {}\n", ec1.message());
        return -1;
    }

    fs::copy_file(live_instrument_dirname / bitbin, libfw / bitbin,
                  fs::copy_options::overwrite_existing, ec2);
    if (ec2) {
        logf<ERROR>("FpgaManager: copy_firmware: '{}' copy failed: {}\n", bitbin, ec2.message());
        return -1;
    }
    return 0;
}

int FpgaManager::clean_up_previous_overlays() {
    if (!fs::exists(kOverlaysRoot)) return 0; // nothing to do
    if (fs::directory_iterator(kOverlaysRoot) == fs::directory_iterator{}) return 0;

    // Release drivers that may keep overlay busy (quiet if absent)
    (void)unbind_all("uio_pdrv_genirq");

    region_enable(false); // best-effort

    // Remove all overlay dirs (reverse order); retry a few times
    std::vector<fs::path> dirs;
    for (const auto& e : fs::directory_iterator(kOverlaysRoot))
        if (fs::is_directory(e)) dirs.push_back(e.path());
    std::sort(dirs.rbegin(), dirs.rend());

    for (int pass = 0; pass < 5; ++pass) {
        for (const auto& d : dirs) {
            if (!fs::exists(d)) continue;
            (void)::rmdir(d.c_str()); // if busy, try again next pass
        }
        if (fs::directory_iterator(kOverlaysRoot) == fs::directory_iterator{}) break;
        usleep(100 * 1000);
    }

    region_enable(true);

    if (fs::directory_iterator(kOverlaysRoot) != fs::directory_iterator{}) {
        log<PANIC>("FpgaManager: overlays still present; cannot proceed\n");
        return -1;
    }
    return 0;
}

int FpgaManager::mount_configfs() {
    if (!fs::exists(kCfgfsRoot)) {
        std::error_code ec;
        fs::create_directories(kCfgfsRoot, ec);
        if (ec) {
            logf<ERROR>("FpgaManager: mount_configfs: mkdir '{}' failed: {}\n", kCfgfsRoot, ec.message());
            return -1;
        }
    }
    if (!fs::exists(kCfgfsDT)) {
        if (mount("none", kCfgfsRoot.c_str(), "configfs", 0, nullptr) < 0) {
            logf<ERROR>("FpgaManager: mount_configfs: mount failed ({}: {})\n", errno, strerror(errno));
            return -1;
        }
    }
    return 0;
}

int FpgaManager::setup_overlay_path() {
    if (mount_configfs() < 0) {
        log<ERROR>("FpgaManager: setup_overlay_path: mount_configfs failed\n");
        return -1;
    }
    if (clean_up_previous_overlays() < 0) {
        log<ERROR>("FpgaManager: setup_overlay_path: cleanup failed\n");
        return -1;
    }
    std::error_code ec;
    fs::create_directories(overlay_path, ec);
    if (ec) {
        logf<ERROR>("FpgaManager: setup_overlay_path: mkdir '{}' failed: {}\n", overlay_path, ec.message());
        return -1;
    }
    if (!fs::exists(overlay_path)) {
        logf<ERROR>("FpgaManager: setup_overlay_path: '{}' not present after mkdir\n", overlay_path);
        return -1;
    }
    return 0;
}

int FpgaManager::setup_fmanager_flags() {
    FILE *xflag = fopen(fmanager_flags.c_str(), "w");
    if (!xflag) {
        logf<ERROR>("FpgaManager: setup_fmanager_flags: open('{}') failed ({}: {})\n",
                                  fmanager_flags, errno, strerror(errno));
        return -1;
    }
    if (fwrite("0", 1, 1, xflag) != 1) {
        logf<ERROR>("FpgaManager: setup_fmanager_flags: write('{}') failed ({}: {})\n",
                                  fmanager_flags, errno, strerror(errno));
        fclose(xflag);
        return -1;
    }
    fclose(xflag);
    return 0;
}

int FpgaManager::write_overlay() {
    const fs::path overlay = overlay_path / "path";
    FILE *f = fopen(overlay.c_str(), "w");
    if (!f) {
        logf<ERROR>("FpgaManager: write_overlay: open('{}') failed ({}: {})\n",
                                  overlay, errno, strerror(errno));
        return -1;
    }
    const std::string echo = "pl.dtbo\n";
    if (fwrite(echo.c_str(), echo.size(), 1, f) != 1) {
        logf<ERROR>("FpgaManager: write_overlay: write('{}') failed\n", overlay);
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

// -----------------------------------------------------------------------------
// Overlay flow
// -----------------------------------------------------------------------------

int FpgaManager::load_bitstream_overlay() {
    // upfront info line (what we're loading)
    const auto bitbin = fs::path{INSTRUMENT_NAME ".bit.bin"};
    logf<INFO>("FpgaManager: Loading {}\n", bitbin);

    if (copy_firmware() < 0) {
        logf<ERROR>("FpgaManager: copy_firmware() failed — could not install 'pl.dtbo' or '{}' into /lib/firmware\n", bitbin);
        return -1;
    }
    if (setup_overlay_path() < 0) {
        logf<ERROR>("FpgaManager: setup_overlay_path() failed — cannot prepare overlay dir '{}'\n", overlay_path);
        return -1;
    }
    if (setup_fmanager_flags() < 0) {
        logf<ERROR>("FpgaManager: setup_fmanager_flags() failed — cannot write flags at '{}'\n", fmanager_flags);
        return -1;
    }
    if (write_overlay() < 0) {
        logf<ERROR>("FpgaManager: write_overlay() failed — cannot write 'pl.dtbo' to '{}/path'\n", overlay_path);
        return -1;
    }

    int rc = check_bitstream_loaded(overlay_fpga_done, 'a');
    if (rc != 0) {
        logf<ERROR>("FpgaManager: overlay apply failed — expected status 'a' in '{}'\n", overlay_fpga_done);
        return rc;
    }

    // UIO: per-device mapping lines
    bind_and_log_uio_mappings();

    // mem_wc (only prints mappings if such devices exist)
    log_mem_wc_mappings();

    return rc;
}

// -----------------------------------------------------------------------------
// xdevcfg flow (legacy)
// -----------------------------------------------------------------------------

int FpgaManager::read_bitstream_data(std::vector<char>& bitstream_data) {
    const auto bit = fs::path{INSTRUMENT_NAME ".bit"};
    const auto filename = live_instrument_dirname / bit;
    FILE *f = fopen(filename.c_str(), "rb");
    if (!f) {
        logf<ERROR>("FpgaManager: read_bitstream_data: open('{}') failed ({}: {})\n",
                                  filename, errno, strerror(errno));
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        logf<ERROR>("FpgaManager: read_bitstream_data: fseek end failed\n");
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz <= 0) {
        logf<ERROR>("FpgaManager: read_bitstream_data: invalid size {}\n", sz);
        fclose(f);
        return -1;
    }
    rewind(f);

    bitstream_data.resize(static_cast<size_t>(sz));
    if (fread(bitstream_data.data(), bitstream_data.size(), 1, f) != 1) {
        logf<ERROR>("FpgaManager: read_bitstream_data: fread failed\n");
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int FpgaManager::load_bitstream_xdevcfg() {
    FILE *xdevcfg = fopen(xdev.c_str(), "w");
    if (!xdevcfg) {
        logf<ERROR>("FpgaManager: load_bitstream_xdevcfg: open('{}') failed ({}: {})\n",
                                  xdev, errno, strerror(errno));
        return -1;
    }

    std::vector<char> buf;
    if (read_bitstream_data(buf) < 0) {
        log<ERROR>("FpgaManager: load_bitstream_xdevcfg: read_bitstream_data failed\n");
        fclose(xdevcfg);
        return -1;
    }

    if (fwrite(buf.data(), buf.size(), 1, xdevcfg) != 1) {
        logf<ERROR>("FpgaManager: load_bitstream_xdevcfg: write '{}' failed\n", xdev);
        fclose(xdevcfg);
        return -1;
    }
    fclose(xdevcfg);

    int rc = check_bitstream_loaded(xdev_fpga_done, '1');
    if (rc != 0) {
        logf<ERROR>("FpgaManager: xdevcfg apply failed — expected status '1' in '{}'\n", xdev_fpga_done);
    }
    return rc;
}

} // namespace hw
