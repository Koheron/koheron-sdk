// Cortex-R5 remoteproc controller
// (c) Koheron

#include "server/hardware/remoteproc_manager.hpp"

#include "server/runtime/syslog.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>

namespace hw {

namespace fs = std::filesystem;

namespace {
constexpr std::chrono::milliseconds kPollInterval{100};
constexpr int kMaxPollAttempts = 50;
} // namespace

std::string RpuFirmwareManager::trim(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    auto begin = std::find_if_not(value.begin(), value.end(), is_space);
    auto end = std::find_if_not(value.rbegin(), value.rend(), is_space).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

bool RpuFirmwareManager::write_sysfs(const Path& path, std::string_view value) {
    if (FILE* f = fopen(path.c_str(), "w")) {
        if (!value.empty()) {
            if (fwrite(value.data(), value.size(), 1, f) != 1) {
                logf<ERROR>("RpuFirmwareManager: write '{}' failed\n", path);
                fclose(f);
                return false;
            }
            if (value.back() != '\n') {
                static constexpr char nl = '\n';
                (void)fwrite(&nl, 1, 1, f);
            }
        } else {
            static constexpr char nl = '\n';
            (void)fwrite(&nl, 1, 1, f);
        }
        fclose(f);
        return true;
    }
    logf<ERROR>("RpuFirmwareManager: open('{}') failed ({}: {})\n", path, errno, strerror(errno));
    return false;
}

std::string RpuFirmwareManager::read_file_trimmed(const Path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        return {};
    }
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return trim(std::move(data));
}

bool RpuFirmwareManager::wait_for_state(const Path& state_path, std::string_view desired,
                                        std::string_view context) const {
    for (int i = 0; i < kMaxPollAttempts; ++i) {
        auto state = read_file_trimmed(state_path);
        if (state == desired) {
            return true;
        }
        std::this_thread::sleep_for(kPollInterval);
    }
    auto final_state = read_file_trimmed(state_path);
    logf<ERROR>("RpuFirmwareManager: {}: state did not reach '{}' (last '{}')\n",
                context, desired, final_state);
    return false;
}

int RpuFirmwareManager::install_manifest_entries() {
    if (!fs::exists(firmware_manifest_)) {
        return 0; // Nothing to install
    }

    std::ifstream manifest(firmware_manifest_);
    if (!manifest.good()) {
        logf<ERROR>("RpuFirmwareManager: cannot open firmware manifest '{}'\n", firmware_manifest_);
        return -1;
    }

    int rc = 0;
    std::string line;
    std::size_t line_no = 0;

    while (std::getline(manifest, line)) {
        ++line_no;
        auto cleaned = trim(std::move(line));
        if (cleaned.empty() || cleaned.front() == '#') {
            continue;
        }
        std::istringstream iss(cleaned);
        std::string src;
        std::string dest;
        if (!(iss >> src >> dest)) {
            logf<ERROR>("RpuFirmwareManager: firmware.manifest:{} malformed entry '{}'\n", line_no, cleaned);
            rc = -1;
            continue;
        }

        const auto src_path = live_instrument_dirname / src;
        if (!fs::exists(src_path)) {
            logf<ERROR>("RpuFirmwareManager: missing firmware '{}', expected at '{}'\n", src, src_path);
            rc = -1;
            continue;
        }

        const auto dest_path = firmware_root_ / dest;
        std::error_code ec;
        if (!dest_path.parent_path().empty()) {
            fs::create_directories(dest_path.parent_path(), ec);
            if (ec) {
                logf<ERROR>("RpuFirmwareManager: cannot create directory '{}': {}\n",
                            dest_path.parent_path(), ec.message());
                rc = -1;
                continue;
            }
        }

        fs::copy_file(src_path, dest_path, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            logf<ERROR>("RpuFirmwareManager: copy '{}' -> '{}' failed: {}\n",
                        src_path, dest_path, ec.message());
            rc = -1;
            continue;
        }
        logf<INFO>("RpuFirmwareManager: installed '{}' to '{}'\n", src, dest_path);
    }

    return rc;
}

int RpuFirmwareManager::refresh_bindings() {
    bindings_.clear();
    bindings_loaded_ = true;

    if (!fs::exists(remoteproc_manifest_)) {
        return 0;
    }

    std::ifstream manifest(remoteproc_manifest_);
    if (!manifest.good()) {
        logf<ERROR>("RpuFirmwareManager: cannot open remoteproc manifest '{}'\n", remoteproc_manifest_);
        return -1;
    }

    int rc = 0;
    std::string line;
    std::size_t line_no = 0;
    while (std::getline(manifest, line)) {
        ++line_no;
        auto cleaned = trim(std::move(line));
        if (cleaned.empty() || cleaned.front() == '#') {
            continue;
        }
        std::istringstream iss(cleaned);
        RemoteProcBinding binding;
        if (!(iss >> binding.remoteproc >> binding.firmware)) {
            logf<ERROR>("RpuFirmwareManager: remoteproc.manifest:{} malformed entry '{}'\n",
                        line_no, cleaned);
            rc = -1;
            continue;
        }
        bindings_.push_back(std::move(binding));
    }

    return rc;
}

int RpuFirmwareManager::set_remoteproc_firmware(const RemoteProcBinding& binding) {
    const auto base = remoteproc_root_ / binding.remoteproc;
    if (!fs::exists(base)) {
        logf<ERROR>("RpuFirmwareManager: remoteproc '{}' not present at '{}'\n", binding.remoteproc, base);
        return -1;
    }
    const auto firmware_path = base / "firmware";
    if (!write_sysfs(firmware_path, binding.firmware)) {
        logf<ERROR>("RpuFirmwareManager: failed to set firmware '{}' for '{}'\n",
                    binding.firmware, binding.remoteproc);
        return -1;
    }
    return 0;
}

int RpuFirmwareManager::stop_binding(const std::string& remoteproc_id) {
    const auto base = remoteproc_root_ / remoteproc_id;
    const auto state_path = base / "state";
    if (!fs::exists(state_path)) {
        return 0; // Nothing to do
    }

    auto current = read_file_trimmed(state_path);
    if (current.empty()) {
        logf<ERROR>("RpuFirmwareManager: cannot read state for '{}'\n", remoteproc_id);
        return -1;
    }

    if (current == "offline") {
        return 0;
    }

    if (!write_sysfs(state_path, "stop")) {
        logf<ERROR>("RpuFirmwareManager: failed to stop '{}'\n", remoteproc_id);
        return -1;
    }

    std::string context = remoteproc_id + " stop";
    if (!wait_for_state(state_path, "offline", context)) {
        return -1;
    }
    logf<INFO>("RpuFirmwareManager: '{}' stopped\n", remoteproc_id);
    return 0;
}

int RpuFirmwareManager::start_binding(const RemoteProcBinding& binding) {
    const auto base = remoteproc_root_ / binding.remoteproc;
    const auto state_path = base / "state";
    if (!fs::exists(state_path)) {
        logf<ERROR>("RpuFirmwareManager: remoteproc '{}' missing 'state' node\n", binding.remoteproc);
        return -1;
    }

    if (!write_sysfs(state_path, "start")) {
        logf<ERROR>("RpuFirmwareManager: failed to start '{}'\n", binding.remoteproc);
        return -1;
    }

    std::string context = binding.remoteproc + " start";
    if (!wait_for_state(state_path, "running", context)) {
        return -1;
    }

    logf<INFO>("RpuFirmwareManager: '{}' running firmware '{}'\n",
               binding.remoteproc, binding.firmware);
    return 0;
}

int RpuFirmwareManager::install_all() {
    return install_manifest_entries();
}

int RpuFirmwareManager::start_all() {
    if (refresh_bindings() < 0) {
        return -1;
    }
    int rc = 0;
    for (const auto& binding : bindings_) {
        if (stop_binding(binding.remoteproc) < 0) {
            rc = -1;
            continue;
        }
        if (set_remoteproc_firmware(binding) < 0) {
            rc = -1;
            continue;
        }
        if (start_binding(binding) < 0) {
            rc = -1;
        }
    }
    return rc;
}

int RpuFirmwareManager::stop_all() {
    if (!bindings_loaded_) {
        if (refresh_bindings() < 0) {
            return -1;
        }
    }
    int rc = 0;
    for (const auto& binding : bindings_) {
        if (stop_binding(binding.remoteproc) < 0) {
            rc = -1;
        }
    }
    return rc;
}

int RpuFirmwareManager::start(std::string_view remoteproc_id) {
    if (refresh_bindings() < 0) {
        return -1;
    }
    auto it = std::find_if(bindings_.begin(), bindings_.end(),
                           [&](const RemoteProcBinding& b) { return b.remoteproc == remoteproc_id; });
    if (it == bindings_.end()) {
        logf<ERROR>("RpuFirmwareManager: no firmware entry for '{}'\n", remoteproc_id);
        return -1;
    }
    if (stop_binding(it->remoteproc) < 0) {
        return -1;
    }
    if (set_remoteproc_firmware(*it) < 0) {
        return -1;
    }
    return start_binding(*it);
}

int RpuFirmwareManager::stop(std::string_view remoteproc_id) {
    return stop_binding(std::string(remoteproc_id));
}

std::vector<std::string> RpuFirmwareManager::known_remoteprocs() {
    if (!bindings_loaded_) {
        (void)refresh_bindings();
    }
    std::vector<std::string> ids;
    ids.reserve(bindings_.size());
    for (const auto& binding : bindings_) {
        ids.push_back(binding.remoteproc);
    }
    return ids;
}

} // namespace hw
