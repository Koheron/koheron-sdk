// Full production loader with filesystem fixtures and captured kernel operations.
#include "server/hardware/fpga_manager.hpp"
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;
namespace {
fs::path root, flags_path, firmware_path, state_path, overlay_path, xdev_path;
fs::path denied_path, failed_write_path, short_write_path, failed_close_path;
std::map<int, fs::path> descriptors;
std::map<fs::path, unsigned> writes;
unsigned mounts = 0, removals = 0, nodes = 0;
bool fail_mount = false, busy_old = false, fail_cleanup = false;
bool bad_state = false, missing_state = false, false_applied = false, bad_overlay_status = false;
int write_error = ENOSPC;
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
void put(const fs::path& path, std::string_view value) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    output.close();
    check(!output.fail(), "fixture write failed");
}
std::string get(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}
void scoped_path(const fs::path& path) {
    check(path.string().starts_with(root.string() + "/"), "loader accessed paths outside the fixture");
}
}

extern "C" int __real_open(const char*, int, ...);
extern "C" int __real_close(int);
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" int __real_rmdir(const char*);
extern "C" int __wrap_open(const char* name, int flags, ...) {
    const fs::path path{name};
    scoped_path(path);
    check((flags & O_CLOEXEC) != 0 && (flags & O_CREAT) == 0, "attribute open flags changed");
    if (path == denied_path) { errno = EACCES; return -1; }
    // configfs materializes attributes when its overlay directory is created.
    if (path == overlay_path / "path" && !fs::exists(path)) {
        put(path, ""); put(overlay_path / "status", "unapplied\n");
    }
    const int fd = __real_open(name, flags);
    if (fd >= 0) descriptors.emplace(fd, path);
    return fd;
}
extern "C" int __wrap_open64(const char* path, int flags, ...) { return __wrap_open(path, flags); }
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t size) {
    const auto found = descriptors.find(fd);
    if (found == descriptors.end()) return __real_write(fd, data, size);
    const auto path = found->second;
    ++writes[path];
    if (path == failed_write_path) { errno = write_error; return -1; }
    if (path == short_write_path) return __real_write(fd, data, size - 1);
    const auto written = __real_write(fd, data, size);
    if (written < 0) return written;
    if (path == firmware_path) {
        if (missing_state) fs::remove(state_path);
        else put(state_path, bad_state ? "write error\n" : "operating\n");
    }
    if (path == overlay_path / "path") {
        put(overlay_path / "status", bad_overlay_status ? "unapplied\n" : "applied\n");
        if (false_applied) put(path, "\n");
    }
    return written;
}
extern "C" int __wrap_close(int fd) {
    const auto found = descriptors.find(fd);
    if (found == descriptors.end()) return __real_close(fd);
    const auto path = found->second;
    descriptors.erase(found);
    const int result = __real_close(fd);
    if (path == failed_close_path) { errno = ENOSPC; return -1; }
    errno = EALREADY; // Cleanup must not replace the original kernel error.
    return result;
}
extern "C" int __wrap_rmdir(const char* name) {
    const fs::path path{name};
    scoped_path(path);
    ++removals;
    if ((busy_old && path.filename() == "old") || (fail_cleanup && path == overlay_path)) {
        errno = EBUSY; return -1;
    }
    std::error_code ec;
    fs::remove(path / "path", ec); fs::remove(path / "status", ec);
    const int result = __real_rmdir(name);
    if (result == 0) errno = EALREADY;
    return result;
}
extern "C" int __wrap_mount(const char*, const char* target, const char* type, unsigned long, const void*) {
    scoped_path(target);
    check(std::string_view(type) == "configfs", "mount type changed");
    ++mounts;
    if (fail_mount) { errno = EPERM; return -1; }
    fs::create_directories(fs::path(target) / "device-tree/overlays");
    return 0;
}
extern "C" int __wrap_mknod(const char* path, mode_t, dev_t) {
    scoped_path(path);
    ++nodes;
    put(path, "synthetic device");
    return 0;
}

int main(int argc, char** argv) {
    check(argc == 3, "fixture root and case required");
    root = argv[1];
    const std::string_view test{argv[2]};
    hw::FpgaManager::Paths paths{root / "live", root / "sys", root / "dev", root / "firmware"};
    flags_path = paths.sys / "class/fpga_manager/fpga0/flags";
    firmware_path = paths.sys / "class/fpga_manager/fpga0/firmware";
    state_path = paths.sys / "class/fpga_manager/fpga0/state";
    const auto overlays = paths.sys / "kernel/config/device-tree/overlays";
    overlay_path = overlays / "full";
    xdev_path = paths.dev / "xdevcfg";
    const auto done = paths.sys / "bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done";
    const auto parameter = paths.sys / "module/firmware_class/parameters/path";
    const auto region = paths.sys / "class/fpga_region/region0/enable";
    const std::string bitstream{"BIT\0IMAGE", 9};
    fs::create_directories(paths.dev);
    fs::create_directories(overlays);
    put(paths.live / "test.bit", bitstream);
    put(paths.live / "test.bit.bin", "raw bitstream");
    put(paths.live / "pl.dtbo", "device overlay");
    put(parameter, paths.live.string() + "\n");
    const bool legacy = test.starts_with("xdev-");
    if (legacy) {
        put(xdev_path, ""); put(done, "1\n");
    }
    if (test != "no-backend" && test != "backend-error" && test != "backend-retry") {
        // Also expose overlay for xdevcfg tests: xdevcfg keeps precedence.
        put(flags_path, "1\n"); put(firmware_path, ""); put(state_path, "reset\n");
    }
    std::string_view stage;
    fs::path failure_path;
    std::error_code expected_code;
    bool success = false;
    if (test == "no-backend" || test == "backend-retry") {
        stage = "select backend"; failure_path = flags_path;
        expected_code = std::make_error_code(std::errc::no_such_device);
    } else if (test == "backend-error") {
        fs::create_symlink(xdev_path, xdev_path);
        stage = "select backend"; failure_path = xdev_path;
        expected_code = std::make_error_code(std::errc::too_many_symbolic_link_levels);
    } else if (test == "overlay-good" || test == "overlay-mounted" || test == "overlay-replaces-old" ||
               test == "overlay-bad-uio" || test == "xdev-good") {
        success = true;
        if (test == "overlay-mounted") fs::remove_all(paths.sys / "kernel/config");
        if (test == "overlay-replaces-old") {
            put(overlays / "old/path", "old.dtbo\n"); put(overlays / "old/status", "applied\n");
            put(region, "1");
        }
        if (test == "overlay-bad-uio") {
            fs::create_directories(paths.sys / "bus/platform/drivers/uio_pdrv_genirq");
            fs::create_directories(paths.sys / "bus/platform/devices");
            put(paths.sys / "class/uio/uio0/dev", "abc:def\n");
        }
    } else if (test == "overlay-missing-payload" || test == "overlay-empty-payload") {
        stage = "prepare firmware"; failure_path = paths.live / "pl.dtbo";
        if (test == "overlay-missing-payload") fs::remove(failure_path); else put(failure_path, "");
    } else if (test == "overlay-copy-error") {
        put(parameter, (root / "custom-firmware").string()); put(paths.firmware, "not a directory");
        stage = "prepare firmware"; failure_path = paths.firmware;
    } else if (test == "overlay-bridges") {
        failure_path = paths.sys / "class/fpga_bridge"; put(failure_path / "bridge0", "bridge");
        stage = "check FPGA bridges"; expected_code = std::make_error_code(std::errc::operation_not_supported);
    } else if (test == "overlay-missing-interface") {
        fs::remove(firmware_path); stage = "check firmware interface"; failure_path = firmware_path;
        expected_code = std::make_error_code(std::errc::no_such_file_or_directory);
    } else if (test == "overlay-configfs-error") {
        fs::remove_all(paths.sys / "kernel"); put(paths.sys / "kernel", "not a directory");
        stage = "prepare configfs"; failure_path = paths.sys / "kernel/config";
        expected_code = std::make_error_code(std::errc::not_a_directory);
    } else if (test == "overlay-mount-error") {
        fs::remove_all(paths.sys / "kernel/config"); fail_mount = true;
        stage = "mount configfs"; failure_path = paths.sys / "kernel/config";
        expected_code = std::make_error_code(std::errc::operation_not_permitted);
    } else if (test == "overlay-inspection-error") {
        fs::remove_all(overlays); put(overlays, "not a directory");
        stage = "inspect old overlays"; failure_path = overlays;
        expected_code = std::make_error_code(std::errc::not_a_directory);
    } else if (test == "overlay-busy") {
        put(overlays / "old/path", "old.dtbo\n"); put(region, "1"); busy_old = true;
        stage = "remove old overlay"; failure_path = overlays / "old";
        expected_code = std::make_error_code(std::errc::device_or_resource_busy);
    } else if (test == "overlay-flags-error") {
        denied_path = flags_path; stage = "set FPGA flags"; failure_path = flags_path;
        expected_code = std::make_error_code(std::errc::permission_denied);
    } else if (test == "overlay-program-error" || test == "overlay-program-short" ||
               test == "overlay-program-close" || test == "overlay-cleanup-error") {
        stage = "program bitstream"; failure_path = firmware_path;
        expected_code = std::make_error_code(std::errc::no_space_on_device);
        if (test == "overlay-program-short") {
            short_write_path = firmware_path; expected_code = std::make_error_code(std::errc::io_error);
        } else if (test == "overlay-program-close") failed_close_path = firmware_path;
        else failed_write_path = firmware_path;
        if (test == "overlay-cleanup-error") fail_cleanup = true;
    } else if (test == "overlay-state-error" || test == "overlay-state-missing") {
        bad_state = test == "overlay-state-error"; missing_state = !bad_state;
        stage = "verify programming"; failure_path = state_path;
        expected_code = bad_state ? std::make_error_code(std::errc::io_error)
                                 : std::make_error_code(std::io_errc::stream);
    } else if (test == "overlay-apply-error") {
        failed_write_path = overlay_path / "path";
        stage = "apply overlay"; failure_path = failed_write_path;
        expected_code = std::make_error_code(std::errc::no_space_on_device);
    } else if (test == "overlay-false-applied" || test == "overlay-status-error") {
        false_applied = test == "overlay-false-applied"; bad_overlay_status = !false_applied;
        stage = false_applied ? "verify overlay path" : "verify overlay status";
        failure_path = overlay_path / (false_applied ? "path" : "status");
        expected_code = std::make_error_code(std::errc::io_error);
    } else if (test == "xdev-missing-bit" || test == "xdev-empty-bit") {
        stage = "read bitstream"; failure_path = paths.live / "test.bit";
        if (test == "xdev-missing-bit") fs::remove(failure_path); else put(failure_path, "");
    } else if (test == "xdev-write-error" || test == "xdev-write-short" || test == "xdev-write-close") {
        stage = "program xdevcfg"; failure_path = xdev_path;
        expected_code = std::make_error_code(std::errc::no_space_on_device);
        if (test == "xdev-write-short") {
            short_write_path = xdev_path; expected_code = std::make_error_code(std::errc::io_error);
        } else if (test == "xdev-write-close") failed_close_path = xdev_path;
        else failed_write_path = xdev_path;
    } else if (test == "xdev-state-error" || test == "xdev-state-missing") {
        stage = "verify xdevcfg programming"; failure_path = done;
        if (test == "xdev-state-error") { put(done, "0\n"); expected_code = std::make_error_code(std::errc::io_error); }
        else { fs::remove(done); expected_code = std::make_error_code(std::io_errc::stream); }
    } else check(false, "unknown case");

    hw::FpgaManager::Result result;
    {
        hw::FpgaManager manager{paths};
        result = manager.load_bitstream_checked();
        if (test == "backend-retry") {
            check(!result && result.error().stage == stage, "missing backend did not return an error");
            put(flags_path, "1\n"); put(firmware_path, ""); put(state_path, "reset\n");
            result = manager.load_bitstream_checked(); success = true;
        }
    }
    check(descriptors.empty(), "loader leaked a descriptor");
    check(result.has_value() == success, "load status differed from expected");
    if (!result) {
        const auto& error = result.error();
        check(error.stage == stage && error.path == failure_path, "load stage or path lost");
        check(bool(error.code), "load error has no code");
        if (expected_code) check(error.code == expected_code, "original load error code lost");
        if (test == "overlay-cleanup-error") {
            check(error.cleanup && error.cleanup->path == overlay_path
                  && error.cleanup->code == std::errc::device_or_resource_busy,
                  "cleanup replaced or omitted original error");
            fail_cleanup = false; fs::remove_all(overlay_path); fail_cleanup = true;
        } else check(!error.cleanup, "unexpected cleanup failure");
        if (test.starts_with("overlay-program-") || test.starts_with("overlay-state-")) {
            check(writes[firmware_path] == 1 && writes[overlay_path / "path"] == 0,
                  "failed programming retried or applied an overlay");
        }
        if (test == "overlay-apply-error" || test == "overlay-false-applied" || test == "overlay-status-error") {
            check(writes[overlay_path / "path"] == 1 && !fs::exists(overlay_path),
                  "failed overlay retried or left behind");
        }
        if (test == "overlay-busy") check(get(region) == "1" && removals == 5, "cleanup retries or region restore changed");
        if (stage == "prepare firmware" || stage == "read bitstream") {
            check(writes[flags_path] == 0 && writes[firmware_path] == 0 && writes[xdev_path] == 0,
                  "invalid payload programmed hardware");
        }
        if (test.starts_with("xdev-write-")) check(writes[xdev_path] == 1, "failed xdevcfg write retried");
        hw::FpgaManager compatibility{paths};
        const auto before = error.code;
        check(compatibility.load_bitstream() == -1, "legacy API accepted load failure");
        const int legacy_errno = before.category() == std::generic_category() || before.category() == std::system_category()
            ? before.value() : EIO;
        check(errno == legacy_errno, "legacy API lost errno through logging or cleanup");
    } else {
        if (legacy) {
            check(get(xdev_path) == bitstream && writes[xdev_path] == 1
                  && writes[firmware_path] == 0, "xdevcfg precedence or binary payload changed");
        } else {
            check(get(flags_path) == "0\n" && get(firmware_path) == "test.bit.bin\n"
                  && get(overlay_path / "path") == "pl.dtbo\n", "overlay commands changed");
            if (test == "overlay-mounted") check(mounts == 1, "configfs did not mount");
            if (test == "overlay-replaces-old") check(!fs::exists(overlays / "old") && get(region) == "1", "old overlay cleanup changed");
            if (test == "overlay-bad-uio") check(nodes == 0, "malformed device number reached mknod");
        }
    }
    check(descriptors.empty(), "legacy loader leaked a descriptor");
}
